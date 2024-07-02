#include "ProxyHandler.hpp"

std::vector<ProxyHandler*> proxy::ProxyHandler::cachedProxies;

std::vector<ProxyHandler*> ProxyHandler::getProxies() {
    return ProxyHandler::cachedProxies;
}

ProxyHandler* ProxyHandler::create(CCHttpRequest* request) {
    ProxyHandler* instance = new ProxyHandler(request);

    request->retain();
    instance->retain();
    ProxyHandler::registerProxy(instance);
    RequestEvent(instance->getInfo()).post();

    return instance;
}

ProxyHandler* ProxyHandler::create(web::WebRequest* request, const std::string& method, const std::string& url) {
    ProxyHandler* instance = new ProxyHandler(request, method, url);

    instance->retain();
    ProxyHandler::registerProxy(instance);
    RequestEvent(instance->getInfo()).post();

    return instance;
}

void ProxyHandler::resumeRequests() {
    CCHttpClient* instance = CCHttpClient::getInstance();

    for (ProxyHandler* proxy : ProxyHandler::cachedProxies) {
        if (proxy->getInfo()->isPaused() && proxy->m_cocosRequest) {
            instance->send(proxy->m_cocosRequest);
        }
    }
}

void ProxyHandler::forgetProxies() {
    for (size_t i = 1; i < ProxyHandler::cachedProxies.size(); i++) {
        delete ProxyHandler::cachedProxies.at(i);
    }

    if (ProxyHandler::cachedProxies.size() > 1) {
        ProxyHandler::cachedProxies.resize(1);
    }
}

void ProxyHandler::resetCache() {
    for (ProxyHandler* proxy : ProxyHandler::cachedProxies) {
        proxy->getInfo()->resetCache();
    }
}

void ProxyHandler::registerProxy(ProxyHandler* proxy) {
    if (!Mod::get()->getSettingValue<bool>("remember-requests") && ProxyHandler::cachedProxies.size() > 0) {
        delete ProxyHandler::cachedProxies.at(0);

        ProxyHandler::cachedProxies.resize(0);
    }

    ProxyHandler::cachedProxies.insert(ProxyHandler::cachedProxies.begin(), proxy);
}

ProxyHandler::ProxyHandler(CCHttpRequest* request) : m_info(new HttpInfo(request)),
    m_cocosRequest(request),
    m_originalTarget(request->getTarget()),
    m_originalProxy(request->getSelector()) {
    request->setResponseCallback(this, httpresponse_selector(ProxyHandler::onResponse));
}

ProxyHandler::ProxyHandler(web::WebRequest* request, const std::string& method, const std::string& url) : m_info(new HttpInfo(request, method, url)),
    m_cocosRequest(nullptr),
    m_originalTarget(nullptr),
    m_originalProxy(nullptr) {
    // request->setResponseCallback(this, httpresponse_selector(ProxyHandler::onResponse));
}

ProxyHandler::~ProxyHandler() {
    delete m_info;
}

HttpInfo* ProxyHandler::getInfo() {
    return m_info;
}

CCHttpRequest* ProxyHandler::getCocosRequest() {
    return m_cocosRequest;
}

void ProxyHandler::onResponse(CCHttpClient* client, CCHttpResponse* response) {
    gd::vector<char>* data = response->getResponseData();

    m_info->m_response = std::string(data->begin(), data->end());
    m_info->m_statusCode = response->getResponseCode();
    m_info->m_responseContentType = m_info->determineContentType(m_info->m_response);

    (m_originalTarget->*m_originalProxy)(client, response);
    ResponseEvent(m_info).post();
}