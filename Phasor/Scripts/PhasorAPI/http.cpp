#include "http.h"
#include "../scripting.hpp"
#include "../../Phasor/Globals.h"
#include <boost/network/protocol/http/client.hpp>
#include <list>
#include <string>
#include <map>
#include <memory>

namespace http = boost::network::http;

struct script_request;

typedef std::map<std::string, std::vector<std::string>> HeaderMap;
typedef std::map<std::string, std::string> ValueMap;

// cbf with Lua and enums
static const int HTTP_GET = 0;
static const int HTTP_POST = 1;

static http::client client;
static std::list<std::unique_ptr<script_request>> activeRequests;

bool validateMode(int mode) {
    return mode >= HTTP_GET && mode <= HTTP_POST;
}

struct script_request
{    
    std::shared_ptr<scripting::PhasorScript> script;
    std::string func;
    lua::types::AnyRef userdata;
    boost::network::uri::uri uri;
    http::client::request request;
    http::client::response response;
    std::string postData;

    script_request(std::shared_ptr<scripting::PhasorScript> script,
                   std::string func, lua::types::AnyRef userdata,
                   boost::network::uri::uri uri)
                   : script(script), func(func), userdata(std::move(userdata)), uri(std::move(uri)),
                     request(this->uri)
    {
    }

    script_request(const script_request&) = delete;
    script_request& operator=(const script_request&) = delete;
    script_request(script_request&&) = delete;
    script_request&& operator=(script_request&&) = delete;

    void get() {
        response = client.get(request);
    }

    void post(std::string data) {
        this->postData = std::move(data);
        response = client.post(request, this->postData);
    }
};


// The Lua push/pop machinery can't handle arbitrary iterator types (even though
// the underlying type of headers() is std::multimap) mainly because there is no
// good is_iterator type trait. So I convert it to some known types that I can then
// specialize.
HeaderMap convertHeaders(http::client::response& response)
{
    HeaderMap output;

    for (auto header : headers(response)) {
        auto itr = output.find(header.first);
        if (itr == output.end()) {
            output.emplace(std::make_pair(header.first, std::vector<std::string>{header.second}));
        } else {
            itr->second.push_back(header.second);
        }
    }

    return output;
}

int l_httpsimple(lua_State* L)
{
    using namespace scripting;
    std::string base_url, callback;
    lua::types::AnyRef userdata;
    boost::optional<int> mode;
    boost::optional<ValueMap> requestData;
    
//    boost::optional<HeaderMap> headers;
    
    // AnyRef can handle nils fine..
    if (lua_gettop(L) == 2)
        lua_pushnil(L);

    std::tie(base_url, callback, userdata, requestData, mode) =
        phlua::callback::getArguments<
            std::string, std::string, decltype(userdata), decltype(requestData),
            decltype(mode)         
        >(L, __FUNCTION__);

    if (!mode)
        mode = HTTP_GET;

    if (base_url.find("https://") == 0)
        luaL_error(L, "cannot perform request : https is not supported");

    // uri class needs to protocol
    if (base_url.find("http://") == std::string::npos)
        base_url.insert(0, "http://");

    boost::network::uri::uri uri(base_url);
    if (!uri.is_valid()) {
        return luaL_argerror(L, 1, "badly formatted url");
    }

    /*
    if (headers) {        
        for (auto p : *headers) {
            for (auto v : p.second)
                request << boost::network::header(p.first, v);
        }
    }*/

    // There's a bug w/ msvc debug where it will complain and incompatible iterators
    // if the query is empty. So we populate it with the uri as a work around.
    boost::network::uri::uri uri_with_query = uri;
    if (requestData) {
        for (auto p : *requestData) {
            uri_with_query << boost::network::uri::query(p.first, boost::network::uri::encoded(p.second));;
        }
    }

    if (mode == HTTP_GET)
        uri = uri_with_query;

    std::shared_ptr<PhasorScript> state = PhasorScript::get(L).shared_from_this();
    std::unique_ptr<script_request> req = std::make_unique<script_request>(state, std::move(callback), std::move(userdata), uri);

    switch (*mode) {
    case HTTP_GET:
        req->get();
        break;
    default: // HTTP_POST:
        req->post(uri_with_query.query());
        break;
    }

    activeRequests.push_back(std::move(req));

    return 0;
}

namespace scripting {
    namespace http_requests {

        void checkRequests() {
            for (auto itr = activeRequests.begin(); itr != activeRequests.end();) {
                script_request& req = **itr;
                if (ready(req.response)) {
                    // process some shit and call the script
                    auto headers = convertHeaders(req.response);
                    std::string data = body(req.response);
                    scripting::Caller<>::call_single(*g_Scripts, *req.script, req.func,
                                                     std::make_tuple(static_cast<std::uint16_t>(status(req.response)),
                                                     std::cref(headers),
                                                     std::cref(data),
                                                     std::cref(req.userdata)));
                    // scripts will be freed when the last reference goes, OnScriptClose is called elsewhere
                    itr = activeRequests.erase(itr);
                } else ++itr;
            }
        }
    }
}