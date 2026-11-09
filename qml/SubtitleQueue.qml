import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var service: null
    property var kodiClient: null
    property var repository: null
    property var appearance: ({})
    property bool webEnabled: false
    property bool open: false
    property int selectedJob: -1
    property string focusArea: "source"
    property bool keyboardOpen: false
    property string editingField: ""
    property int selectedRepositoryResult: 0
    property string repositoryType: "movie"
    signal closeRequested()
    SurfacePalette { id: shell; appearance: root.appearance }

    function currentJob() {
        const values = service ? service.jobs : []
        return selectedJob >= 0 && selectedJob < values.length ? values[selectedJob] : null
    }

    function queueTranslation() {
        if (!service) return
        const id = service.queueTranslation(sourcePath.text, sourceLanguage.text, targetLanguage.text)
        if (id) { selectedJob = service.jobs.length - 1; focusArea = "jobs" }
    }

    function generateFromAudio() {
        if (!service) return
        const id = service.queueGeneration(mediaPath.text, modelPath.text)
        if (id) { selectedJob = service.jobs.length - 1; focusArea = "jobs" }
    }

    function reviewSelected() {
        const job = currentJob()
        if (job && service) reviewText.text = service.reviewText(job.id)
    }

    function currentRepositoryResult() {
        const values = repository ? repository.results : []
        return selectedRepositoryResult >= 0 && selectedRepositoryResult < values.length
            ? values[selectedRepositoryResult] : null
    }

    function importRepositoryResult() {
        const result = currentRepositoryResult()
        if (result && repository) repository.download(result.fileId)
    }

    onOpenChanged: if (!open) keyboardOpen = false

    function handleCommand(command) {
        if (!open) return
        if (keyboardOpen) { keyboard.handleCommand(command); return }
        if (command === inputManager.back) { closeRequested(); return }
        const areas = ["source", "sourceLanguage", "targetLanguage", "queueTranslation",
                       "media", "model", "generate", "languagePackPath", "installPack", "refreshPacks",
                       "onlineQuery", "onlineLanguage", "onlineType", "onlineSearch", "onlineResults", "onlineDownload",
                       "jobs", "cancel", "review", "reviewText", "saveReview", "load", "forget"]
        let index = areas.indexOf(focusArea)
        if (command === inputManager.navigateDown) focusArea = areas[Math.min(areas.length - 1, index + 1)]
        else if (command === inputManager.navigateUp) focusArea = areas[Math.max(0, index - 1)]
        else if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            if (focusArea === "jobs" && service && service.jobs.length)
                selectedJob = Math.max(0, Math.min(service.jobs.length - 1, selectedJob + (command === inputManager.navigateRight ? 1 : -1)))
            else if (focusArea === "onlineResults" && repository && repository.results.length)
                selectedRepositoryResult = Math.max(0, Math.min(repository.results.length - 1,
                    selectedRepositoryResult + (command === inputManager.navigateRight ? 1 : -1)))
            else if (focusArea === "onlineType")
                repositoryType = command === inputManager.navigateRight ? "episode" : "movie"
        } else if (command === inputManager.confirm) {
            if (focusArea === "queueTranslation") queueTranslation()
            else if (focusArea === "onlineSearch") { if (repository && webEnabled) repository.search(onlineQuery.text, onlineLanguage.text, repositoryType) }
            else if (focusArea === "onlineDownload") importRepositoryResult()
            else if (focusArea === "installPack") { if (service) service.installLanguagePack(languagePackPath.text) }
            else if (focusArea === "refreshPacks") { if (service) service.refreshLanguagePacks() }
            else if (focusArea === "generate") generateFromAudio()
            else if (focusArea === "cancel") { const job = currentJob(); if (job && service) service.cancel(job.id) }
            else if (focusArea === "review") { const job = currentJob(); if (job && service) reviewText.text = service.reviewText(job.id) }
            else if (focusArea === "reviewText") {
                editingField = focusArea
                keyboard.value = reviewText.text
                keyboard.selectedIndex = 0
                keyboardOpen = true
            }
            else if (focusArea === "saveReview") { const job = currentJob(); if (job && service) service.saveReview(job.id, reviewText.text) }
            else if (focusArea === "load") { const job = currentJob(); if (job && job.state === "Ready for review" && kodiClient) kodiClient.loadSubtitle(job.output) }
            else if (focusArea === "forget") { const job = currentJob(); if (job && service && service.forgetJob(job.id)) selectedJob = Math.min(selectedJob, service.jobs.length - 1) }
            else if (focusArea === "jobs") {
                const job = currentJob()
                if (job && job.state === "Ready for review") reviewSelected()
            } else {
                const field = focusArea === "source" ? sourcePath
                    : focusArea === "sourceLanguage" ? sourceLanguage
                    : focusArea === "targetLanguage" ? targetLanguage
                    : focusArea === "languagePackPath" ? languagePackPath
                    : focusArea === "onlineQuery" ? onlineQuery
                    : focusArea === "onlineLanguage" ? onlineLanguage
                    : focusArea === "media" ? mediaPath : modelPath
                editingField = focusArea
                keyboard.value = field.text
                keyboard.selectedIndex = 0
                keyboardOpen = true
            }
        }
    }

    visible: open
    z: 1200
    Rectangle { anchors.fill: parent; color: shell.scrim }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 1120)
        height: Math.min(parent.height - 48, 680)
        radius: 18
        color: shell.surface
        border.width: 2
        border.color: shell.focus

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 10
            Text {
                text: "Isolde Linguist · Offline subtitle queue"
                color: shell.text
                font.pixelSize: 28 * shell.textScale
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: shell.muted
                font.pixelSize: 15 * shell.textScale
                text: service ? (service.errorString || service.status) + " · " + service.languagePackStatus + (kodiClient && kodiClient.playbackStatus ? " · Kodi: " + kodiClient.playbackStatus : "")
                    : "Offline tools unavailable. No media or subtitle text is sent online."
            }
            Row {
                width: parent.width
                spacing: 8
                TextField { id: sourcePath; objectName: "linguistSourcePath"; width: parent.width * 0.62; placeholderText: "Existing SRT path"; Accessible.name: "Existing subtitle file path"; background: Rectangle { color:shell.input;border.width:root.focusArea==="source"?3:1;border.color:root.focusArea==="source"?shell.focus:shell.outline } }
                TextField { id: sourceLanguage; objectName: "linguistSourceLanguage"; width: 100; text: "zh"; placeholderText: "Source"; Accessible.name: "Source language code"; background: Rectangle { color:shell.input;border.width:root.focusArea==="sourceLanguage"?3:1;border.color:root.focusArea==="sourceLanguage"?shell.focus:shell.outline } }
                TextField { id: targetLanguage; objectName: "linguistTargetLanguage"; width: 100; text: "en"; placeholderText: "Target"; Accessible.name: "Target language code"; background: Rectangle { color:shell.input;border.width:root.focusArea==="targetLanguage"?3:1;border.color:root.focusArea==="targetLanguage"?shell.focus:shell.outline } }
            }
            Row {
                width: parent.width
                spacing: 8
                Button {
                    id: queueTranslationButton; objectName: "linguistQueueTranslation"
                    text: "Translate existing SRT offline"
                    onClicked: root.queueTranslation()
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="queueTranslation"?3:1;border.color:root.focusArea==="queueTranslation"?shell.focus:shell.outline;radius:8 }
                }
                TextField { id: mediaPath; objectName: "linguistMediaPath"; width: parent.width * 0.28; placeholderText: "Media file path"; Accessible.name: "Media file path for local generation"; background: Rectangle { color:shell.input;border.width:root.focusArea==="media"?3:1;border.color:root.focusArea==="media"?shell.focus:shell.outline } }
                TextField { id: modelPath; objectName: "linguistWhisperModel"; width: parent.width * 0.23; placeholderText: "Installed multilingual whisper.cpp .bin model"; Accessible.name: "Installed local whisper.cpp model path"; background: Rectangle { color:shell.input;border.width:root.focusArea==="model"?3:1;border.color:root.focusArea==="model"?shell.focus:shell.outline } }
                Button {
                    id: generateButton; objectName: "linguistGenerateAudio"
                    text: "Generate English SRT offline"
                    onClicked: root.generateFromAudio()
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="generate"?3:1;border.color:root.focusArea==="generate"?shell.focus:shell.outline;radius:8 }
                }
            }
            Text {
                color: shell.muted
                font.pixelSize: 13 * shell.textScale
                text: "Only selected files are processed. Import Argos .argosmodel packages from local storage; whisper.cpp and its multilingual model must already be installed. No downloads. Output never overwrites a subtitle and requires human review."
            }
            Row {
                width: parent.width
                spacing: 8
                TextField {
                    id: languagePackPath
                    objectName: "linguistLanguagePackPath"
                    width: parent.width * 0.62
                    placeholderText: "Local .argosmodel package path"
                    Accessible.name: "Explicit local Argos language package path"
                    background: Rectangle { color:shell.input;border.width:root.focusArea==="languagePackPath"?3:1;border.color:root.focusArea==="languagePackPath"?shell.focus:shell.outline }
                }
                Button {
                    objectName: "linguistInstallLanguagePack"
                    text: "Import local pack"
                    onClicked: if (root.service) root.service.installLanguagePack(languagePackPath.text)
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="installPack"?3:1;border.color:root.focusArea==="installPack"?shell.focus:shell.outline;radius:8 }
                }
                Button {
                    objectName: "linguistRefreshLanguagePacks"
                    text: "Refresh"
                    onClicked: if (root.service) root.service.refreshLanguagePacks()
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="refreshPacks"?3:1;border.color:root.focusArea==="refreshPacks"?shell.focus:shell.outline;radius:8 }
                }
            }
            Text {
                width: parent.width
                color: shell.muted
                font.pixelSize: 13 * shell.textScale
                elide: Text.ElideRight
                text: service
                    ? service.languagePackStatus + (service.languagePacks.length
                        ? " · " + service.languagePacks.map(pair => pair.source.toUpperCase() + " → " + pair.target.toUpperCase()).join(", ")
                        : "")
                    : "Argos language pack manager unavailable."
            }
            Row {
                width: parent.width
                spacing: 8
                TextField {
                    id: onlineQuery
                    objectName: "linguistRepositoryQuery"
                    width: parent.width * 0.53
                    placeholderText: "Search repository by title"
                    Accessible.name: "OpenSubtitles title query; no media is uploaded"
                    background: Rectangle { color:shell.input;border.width:root.focusArea==="onlineQuery"?3:1;border.color:root.focusArea==="onlineQuery"?shell.focus:shell.outline }
                }
                TextField {
                    id: onlineLanguage
                    objectName: "linguistRepositoryLanguage"
                    width: 76
                    text: "en"
                    placeholderText: "Language"
                    Accessible.name: "Subtitle language code"
                    background: Rectangle { color:shell.input;border.width:root.focusArea==="onlineLanguage"?3:1;border.color:root.focusArea==="onlineLanguage"?shell.focus:shell.outline }
                }
                Button {
                    objectName: "linguistRepositoryType"
                    text: repositoryType
                    Accessible.name: "Subtitle repository media type"
                    onClicked: root.repositoryType = root.repositoryType === "movie" ? "episode" : "movie"
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="onlineType"?3:1;border.color:root.focusArea==="onlineType"?shell.focus:shell.outline;radius:8 }
                }
                Button {
                    objectName: "linguistRepositorySearch"
                    text: "Search"
                    enabled: root.webEnabled && root.repository && root.repository.configured && !root.repository.busy
                    onClicked: if (root.repository && root.webEnabled) root.repository.search(onlineQuery.text, onlineLanguage.text, root.repositoryType)
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="onlineSearch"?3:1;border.color:root.focusArea==="onlineSearch"?shell.focus:shell.outline;radius:8 }
                }
            }
            Row {
                width: parent.width
                spacing: 8
                Text {
                    width: parent.width - 178
                    height: 42
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: shell.muted
                    font.pixelSize: 13 * shell.textScale
                    text: {
                        const result = root.currentRepositoryResult()
                        if (!root.webEnabled) return "Web Off · no subtitle repository requests."
                        if (!root.repository) return "OpenSubtitles adapter unavailable."
                        if (!result) return root.repository.status
                        return "OpenSubtitles · " + (root.selectedRepositoryResult + 1) + "/" +
                            root.repository.results.length + " · " + result.title + " · " +
                            result.language + " · " + result.fileName + " · " +
                            result.downloads + " downloads · Enter downloads this result"
                    }
                }
                Button {
                    objectName: "linguistRepositoryDownload"
                    text: "Download SRT"
                    enabled: root.webEnabled && root.repository && !root.repository.busy && !!root.currentRepositoryResult()
                    onClicked: root.importRepositoryResult()
                    background: Rectangle { color:shell.surface;border.width:root.focusArea==="onlineDownload"?3:1;border.color:root.focusArea==="onlineDownload"?shell.focus:shell.outline;radius:8 }
                }
            }
            Text {
                width: parent.width
                color: shell.muted
                font.pixelSize: 12 * shell.textScale
                text: "Repository downloads are explicit, bounded and stored for review. Only the title/language query is sent; media files and existing subtitles are never uploaded."
            }
            Row {
                width: parent.width
                spacing: 8
                Button { id: cancelButton; objectName: "linguistCancel"; text: "Cancel selected"; onClicked: { const job = root.currentJob(); if (job && root.service) root.service.cancel(job.id) }; background: Rectangle { color:shell.surface;border.width:root.focusArea==="cancel"?3:1;border.color:root.focusArea==="cancel"?shell.focus:shell.outline;radius:8 } }
                Button { id: reviewButton; objectName: "linguistReview"; text: "Review selected"; onClicked: root.reviewSelected(); background: Rectangle { color:shell.surface;border.width:root.focusArea==="review"?3:1;border.color:root.focusArea==="review"?shell.focus:shell.outline;radius:8 } }
                Button { id: saveReviewButton; objectName: "linguistSaveReview"; text: "Save review"; onClicked: { const job = root.currentJob(); if (job && root.service) root.service.saveReview(job.id, reviewText.text) }; background: Rectangle { color:shell.surface;border.width:root.focusArea==="saveReview"?3:1;border.color:root.focusArea==="saveReview"?shell.focus:shell.outline;radius:8 } }
                Button { id: loadButton; objectName: "linguistLoadKodi"; text: "Load in Kodi"; onClicked: { const job = root.currentJob(); if (job && job.state === "Ready for review" && root.kodiClient) root.kodiClient.loadSubtitle(job.output) }; background: Rectangle { color:shell.surface;border.width:root.focusArea==="load"?3:1;border.color:root.focusArea==="load"?shell.focus:shell.outline;radius:8 } }
                Button { id: forgetButton; objectName: "linguistForget"; text: "Forget job record"; onClicked: { const job = root.currentJob(); if (job && root.service && root.service.forgetJob(job.id)) selectedJob = Math.min(selectedJob, root.service.jobs.length - 1) }; background: Rectangle { color:shell.surface;border.width:root.focusArea==="forget"?3:1;border.color:root.focusArea==="forget"?shell.focus:shell.outline;radius:8 } }
                Button { objectName: "linguistClose"; text: "Close"; onClicked: closeRequested() }
            }
            Row {
                width: parent.width
                height: parent.height - 455
                spacing: 12
                Rectangle {
                    width: parent.width * 0.45
                    height: parent.height
                    radius: 10
                    color: shell.elevated
                    border.width: focusArea === "jobs" ? 2 : 0
                    border.color: shell.focus
                    ListView {
                        id: jobList
                        objectName: "linguistJobs"
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        model: root.service ? root.service.jobs : []
                        onCountChanged: if (count) positionViewAtEnd()
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            width: jobList.width
                            height: 90
                            radius: 8
                            color: index === root.selectedJob ? shell.selected : shell.elevated
                            border.width: index === root.selectedJob ? 2 : 0
                            border.color: shell.focus
                            Column {
                                anchors.fill: parent
                                anchors.margins: 8
                                Text { width: parent.width; text: modelData.mode + " · " + modelData.state + " · " + modelData.progress + "%"; color: shell.text; elide: Text.ElideRight; font.pixelSize: 15 * shell.textScale }
                                Text { width: parent.width; text: modelData.output || modelData.error; color: shell.muted; elide: Text.ElideMiddle; font.pixelSize: 12 * shell.textScale }
                                Text {
                                    width: parent.width
                                    text: modelData.qualityReport && modelData.qualityReport.assessment
                                        ? "Quality: structural/heuristic only · unchanged cues " + modelData.qualityReport.unchangedCuePercent + "% · human review required"
                                        : "Quality evaluation pending"
                                    color: shell.muted
                                    elide: Text.ElideRight
                                    font.pixelSize: 11 * shell.textScale
                                }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { root.selectedJob = index; root.focusArea = "jobs" } }
                        }
                    }
                }
                TextArea {
                    id: reviewText
                    objectName: "linguistReviewText"
                    width: parent.width * 0.55 - 12
                    height: parent.height
                    wrapMode: TextEdit.NoWrap
                    selectByMouse: true
                    placeholderText: "Select a completed job and choose Review selected. Edit the UTF-8 SRT, then save."
                    Accessible.name: "Subtitle review editor"
                    background: Rectangle { color:shell.input;border.width:root.focusArea==="reviewText"?3:1;border.color:root.focusArea==="reviewText"?shell.focus:shell.outline;radius:8 }
                }
            }
            OnScreenKeyboard {
                id: keyboard
                objectName: "linguistKeyboard"
                anchors.centerIn: parent
                width: Math.min(parent.width - 80, 850)
                height: Math.min(parent.height - 60, 440)
                visible: root.open && root.keyboardOpen
                z: 20
                appearance: root.appearance
                onAccepted: text => {
                    if (root.editingField === "source") sourcePath.text = text
                    else if (root.editingField === "sourceLanguage") sourceLanguage.text = text
                    else if (root.editingField === "targetLanguage") targetLanguage.text = text
                    else if (root.editingField === "onlineQuery") onlineQuery.text = text
                    else if (root.editingField === "onlineLanguage") onlineLanguage.text = text
                    else if (root.editingField === "media") mediaPath.text = text
                    else if (root.editingField === "model") modelPath.text = text
                    else if (root.editingField === "reviewText") reviewText.text = text
                    root.keyboardOpen = false
                }
                onCanceled: root.keyboardOpen = false
            }
            Connections {
                target: root.repository
                function onChanged() {
                    const count = root.repository ? root.repository.results.length : 0
                    root.selectedRepositoryResult = count ? Math.min(root.selectedRepositoryResult, count - 1) : 0
                }
                function onSubtitleDownloaded(title, language, srtText, repositoryName, fileId) {
                    if (!root.service) return
                    const id = root.service.importOnlineSubtitle(title, language, srtText, fileId)
                    if (id) {
                        root.selectedJob = root.service.jobs.length - 1
                        root.focusArea = "jobs"
                    }
                }
            }
        }
    }
}
