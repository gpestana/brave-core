/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface GreaselionError {
  errorMessage: string
}

interface OnAPIRequest {
  name: string
  url: string
  init: {}
}

interface MediaDurationMetadata {
  mediaKey: string
  duration: number
  firstVisit: boolean
}

interface RegisterOnCompletedWebRequest {
  urlPatterns: string[]
}

interface RegisterOnSendHeadersWebRequest {
  urlPatterns: string[]
  extra?: string[]
}

interface SavePublisherVisit {
  url: string
  publisherKey: string
  publisherName: string
  mediaKey?: string
  favIconUrl?: string
}

interface TipUser {
  url: string
  publisherKey: string
  publisherName: string
  publisherScreenName: string
  favIconUrl: string
  postId: string
  postTimestamp: string
  postText: string
}

interface OnCompletedWebRequestRegistration {
  registered: boolean
}

interface OnSendHeadersWebRequestRegistration {
  registered: boolean
}

interface OnUpdatedTabRegistration {
  registered: boolean
}

let onCompletedWebRequestBinding =
  (details: chrome.webRequest.WebResponseCacheDetails) => {}

let onSendHeadersWebRequestBinding =
  (details: chrome.webRequest.WebRequestHeadersDetails) => {}

let onUpdatedTabBinding =
  (tabId: number, changeInfo: chrome.tabs.TabChangeInfo, tab: chrome.tabs.Tab) => {}

// Maps webRequest.OnCompleted registrations by tabId:senderId
const onCompletedWebRequestRegistrations =
  new Map<string, OnCompletedWebRequestRegistration>()

// Maps webRequest.OnSendHeaders registrations by tabId:senderId
const onSendHeadersWebRequestRegistrations =
  new Map<string, OnSendHeadersWebRequestRegistration>()

// Maps tabs.OnUpdated registrations by tabId:senderId
const onUpdatedTabRegistrations =
  new Map<string, OnUpdatedTabRegistration>()

// Maps Greaselion ports by tabId:senderId
const portsByTabIdSenderId = new Map<string, chrome.runtime.Port>()

// Maps publisher keys by media key
const publisherKeysByMediaKey = new Map<string, string>()

// Maps publisher keys by tabId
const publisherKeysByTabId = new Map<number, string>()

const braveRewardsExtensionId = 'jidkidbbcafjabdphckchenhfomhnfma'

const buildTabIdSenderIdKey = (tabId: number, senderId: string) => {
  if (!tabId || !senderId) {
    return ''
  }

  return `${tabId}:${senderId}`
}

const handleGreaselionError = (tabId: number, mediaType: string, data: GreaselionError) => {
  console.error(`Greaselion error: ${data.errorMessage}`)
}

const handleOnAPIRequest = (data: OnAPIRequest, onSuccess: (response: any) => void, onFailure: (error: any) => void) => {
  if (!data || !data.url || !data.init || !onSuccess || !onFailure) {
    return
  }

  fetch(data.url, data.init)
    .then(response => {
      if (!response.ok) {
        throw new Error(`API request failed: ${response.statusText} (${response.status})`)
      }

      return response.json()
    })
    .then(responseData => onSuccess(responseData))
    .catch(error => onFailure(error))
}

const handleMediaDurationMetadata = (tabId: number, mediaType: string, data: MediaDurationMetadata) => {
  const publisherKey = publisherKeysByMediaKey.get(data.mediaKey)
  if (!publisherKey) {
    console.error(`Failed to handle media duration metadata: missing publisher key for media key ${data.mediaKey}`)
    return
  }

  chrome.braveRewards.updateMediaDuration(tabId, publisherKey, data.duration, data.firstVisit)
}

const onCompletedWebRequest = (
  registrationKey: string,
  mediaType: string,
  details: chrome.webRequest.WebResponseCacheDetails
) => {
  const port = portsByTabIdSenderId.get(registrationKey)
  if (!port) {
    return
  }
  port.postMessage({
    type: 'OnCompletedWebRequest',
    mediaType,
    details
  })
}

const handleRegisterOnCompletedWebRequest = (
  registrationKey: string,
  mediaType: string,
  data: RegisterOnCompletedWebRequest
) => {
  const handler = onCompletedWebRequestRegistrations.get(registrationKey)
  if (handler && handler.registered) {
    return
  }

  // Mark this tab as registered
  onCompletedWebRequestRegistrations.set(registrationKey, { registered: true })

  // Create and install the listener
  onCompletedWebRequestBinding =
    onCompletedWebRequest.bind(null, registrationKey, mediaType)
  chrome.webRequest.onCompleted.addListener(
    // Listener
    onCompletedWebRequestBinding,
    // Filters
    {
      types: [
        'image',
        'media',
        'script',
        'xmlhttprequest'
      ],
      urls: data.urlPatterns
    })
}

const onSendHeadersWebRequest = (
  registrationKey: string,
  mediaType: string,
  details: chrome.webRequest.WebRequestHeadersDetails
) => {
  const port = portsByTabIdSenderId.get(registrationKey)
  if (!port) {
    return
  }
  port.postMessage({
    type: 'OnSendHeadersWebRequest',
    mediaType,
    data: {
      details
    }
  })
}

const handleRegisterOnSendHeadersWebRequest = (registrationKey: string, mediaType: string, data: RegisterOnSendHeadersWebRequest) => {
  // If we already registered a handler for this tab, exit early
  const handler = onSendHeadersWebRequestRegistrations.get(registrationKey)
  if (handler && handler.registered) {
    return
  }

  // Mark this tab as registered
  onSendHeadersWebRequestRegistrations.set(registrationKey, { registered: true })

  // Create and install the listener
  onSendHeadersWebRequestBinding =
    onSendHeadersWebRequest.bind(null, registrationKey, mediaType)
  chrome.webRequest.onSendHeaders.addListener(
    // Listener
    onSendHeadersWebRequestBinding,
    // Filters
    {
      urls: data.urlPatterns
    },
    // Extra
    data.extra
  )
}

const onUpdatedTab = (
  registrationKey: string,
  mediaType: string,
  tabId: number,
  changeInfo: chrome.tabs.TabChangeInfo,
  tab: chrome.tabs.Tab
) => {
  const port = portsByTabIdSenderId.get(registrationKey)
  if (!port) {
    return
  }
  port.postMessage({
    type: 'OnUpdatedTab',
    mediaType,
    data: {
      tabId,
      changeInfo,
      tab
    }
  })
}

const handleRegisterOnUpdatedTab = (registrationKey: string, mediaType: string) => {
  // If we already registered a handler for this tab, exit early
  const handler = onUpdatedTabRegistrations.get(registrationKey)
  if (handler && handler.registered) {
    return
  }

  // Mark this tab as registered
  onUpdatedTabRegistrations.set(registrationKey, { registered: true })

  // Create and install the listener
  onUpdatedTabBinding = onUpdatedTab.bind(null, registrationKey, mediaType)
  chrome.tabs.onUpdated.addListener(onUpdatedTabBinding)
}

const getPublisherPanelInfo = (tabId: number, publisherKey: string) => {
  chrome.braveRewards.getPublisherPanelInfo(
    publisherKey, (result: RewardsExtension.Result, info?: RewardsExtension.Publisher) => {
      if (result === 0 && info) {
        chrome.runtime.sendMessage(
          braveRewardsExtensionId,
          {
            type: 'OnPublisherData',
            tabId,
            info
          })
      }
      return
    })
}

const getPublisherPanelInfoByTabId = (tabId: number) => {
  if (!tabId) {
    return
  }

  const publisherKey = publisherKeysByTabId.get(tabId)
  if (!publisherKey) {
    return
  }

  getPublisherPanelInfo(tabId, publisherKey)
}

const savePublisherInfo = (tabId: number, mediaType: string, url: string, publisherKey: string, publisherName: string, favIconUrl: string) => {
  chrome.braveRewards.savePublisherInfo(
    tabId,
    mediaType,
    url,
    publisherKey,
    publisherName,
    favIconUrl,
    (result: RewardsExtension.Result) => {
      if (result !== 0) {
        console.error(`Failed to save publisher info for ${publisherKey}, result is ${result}`)
        return
      }
    })
}

const handleSavePublisherVisit = (tabId: number, mediaType: string, data: SavePublisherVisit) => {
  if (!data.publisherKey || !data.publisherName) {
    console.error('Invalid parameter')
    return
  }

  publisherKeysByTabId.set(tabId, data.publisherKey)

  if (data.mediaKey && !publisherKeysByMediaKey.has(data.mediaKey)) {
    publisherKeysByMediaKey.set(data.mediaKey, data.publisherKey)
  }

  chrome.braveRewards.getPublisherInfo(
    data.publisherKey, (result: RewardsExtension.Result, info?: RewardsExtension.Publisher) => {
      if (result === 0 && info) {
        getPublisherPanelInfo(tabId, data.publisherKey)
        return
      }

      // Failed to find publisher info corresponding to this key, so save it now
      if (result === 9) {
        savePublisherInfo(
          tabId,
          mediaType,
          data.url,
          data.publisherKey,
          data.publisherName,
          data.favIconUrl || '')
        return
      }
    })
}

const handleTipUser = (tabId: number, mediaType: string, data: TipUser) => {
  chrome.braveRewards.tipUser(
    tabId,
    mediaType,
    data.url,
    data.publisherKey,
    data.publisherName,
    data.publisherScreenName,
    data.favIconUrl,
    data.postId,
    data.postTimestamp,
    data.postText)
}

const onMessageListener = (msg: any, port: chrome.runtime.Port) => {
  if (!port || !port.sender || !port.sender.id || !port.sender.tab || !msg) {
    return
  }

  const tabId = port.sender.tab.id
  if (!tabId) {
    return
  }

  const senderId = port.sender.id
  if (!senderId) {
    return
  }

  const key = buildTabIdSenderIdKey(tabId, senderId)
  if (!portsByTabIdSenderId.get(key)) {
    portsByTabIdSenderId.set(key, port)
  }

  chrome.greaselion.isGreaselionExtension(port.sender.id, (valid: boolean) => {
    if (!valid) {
      return
    }
    switch (msg.type) {
      case 'GreaselionError': {
        const data = msg.data as GreaselionError
        handleGreaselionError(tabId, msg.mediaType, data)
        break
      }
      case 'MediaDurationMetadata': {
        const data = msg.data as MediaDurationMetadata
        handleMediaDurationMetadata(tabId, msg.mediaType, data)
        break
      }
      case 'OnAPIRequest': {
        const data = msg.data as OnAPIRequest
        handleOnAPIRequest(
          data,
          (response: any) => {
            if (portsByTabIdSenderId.has(key)) {
              port.postMessage({
                type: 'OnAPIResponse',
                mediaType: msg.mediaType,
                data: {
                  name: data.name,
                  response
                }
              })
            }
          },
          (error: any) => {
            if (portsByTabIdSenderId.has(key)) {
              port.postMessage({
                type: 'OnAPIResponse',
                mediaType: msg.mediaType,
                data: {
                  name: data.name,
                  error
                }
              })
            }
          })
        break
      }
      case 'RegisterOnCompletedWebRequest': {
        const data = msg.data as RegisterOnCompletedWebRequest
        handleRegisterOnCompletedWebRequest(key, msg.mediaType, data)
        break
      }
      case 'RegisterOnSendHeadersWebRequest': {
        const data = msg.data as RegisterOnSendHeadersWebRequest
        handleRegisterOnSendHeadersWebRequest(key, msg.mediaType, data)
        break
      }
      case 'RegisterOnUpdatedTab': {
        handleRegisterOnUpdatedTab(key, msg.mediaType)
        break
      }
      case 'SavePublisherVisit': {
        const data = msg.data as SavePublisherVisit
        handleSavePublisherVisit(tabId, msg.mediaType, data)
        break
      }
      case 'TipUser': {
        const data = msg.data as TipUser
        handleTipUser(tabId, msg.mediaType, data)
        break
      }
    }
  })
}

chrome.runtime.onConnectExternal.addListener((port: chrome.runtime.Port) => {
  if (!port || port.name !== 'Greaselion') {
    return
  }

  port.onMessage.addListener(onMessageListener)

  port.onDisconnect.addListener((port: chrome.runtime.Port) => {
    if (chrome.runtime.lastError) {
      console.error(`Greaselion port disconnected due to error: ${chrome.runtime.lastError}`)
    }
    if (port.sender && port.sender.id && port.sender.tab && port.sender.tab.id) {
      port.onMessage.removeListener(onMessageListener)

      publisherKeysByTabId.delete(port.sender.tab.id)

      const key = buildTabIdSenderIdKey(port.sender.tab.id, port.sender.id)

      portsByTabIdSenderId.delete(key)

      onCompletedWebRequestRegistrations.delete(key)
      chrome.webRequest.onCompleted.removeListener(onCompletedWebRequestBinding)

      onSendHeadersWebRequestRegistrations.delete(key)
      chrome.webRequest.onSendHeaders.removeListener(onSendHeadersWebRequestBinding)

      onUpdatedTabRegistrations.delete(key)
      chrome.tabs.onUpdated.removeListener(onUpdatedTabBinding)
    }
  })
})

chrome.runtime.onMessageExternal.addListener(
  function (msg: any, sender: chrome.runtime.MessageSender, sendResponse: any) {
    if (!msg || !msg.type || !sender || !sender.id) {
      return
    }
    chrome.greaselion.isGreaselionExtension(sender.id, (valid: boolean) => {
      if (!valid && sender.id !== braveRewardsExtensionId) {
        return
      }
      switch (msg.type) {
        case 'GetPublisherPanelInfo':
          getPublisherPanelInfoByTabId(msg.tabId)
          break
        case 'SupportsGreaselion':
          sendResponse({ supported: true })
          break
      }
    })
  })
