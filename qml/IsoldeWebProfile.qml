import QtWebEngine

WebEngineProfile {
    offTheRecord: storageName.length === 0
    persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
    httpUserAgent: "Mozilla/5.0 (SMART-TV; Linux; Isolde) AppleWebKit/537.36 (KHTML, like Gecko) Chrome Safari/537.36"
}
