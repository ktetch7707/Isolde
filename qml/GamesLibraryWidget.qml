import QtQuick

// A content adapter inside the shared page library surface. Presentation comes
// from the same Studio appearance map; catalog records never become layout nodes.
Item {
    id:root
    objectName:"gamesLibraryWidget"
    property var library:null
    property var dispatcher:null
    property var providerStatus:({})
    property string originId:""
    property var style:({})
    property var appearance:({})
    property bool engaged:false
    property bool readOnly:false
    property var items:[]
    property int selected:-1
    property int toolbar:0
    property int offset:0
    readonly property int catalogLimit:library?library.catalogLimit:50000
    readonly property int pageSize:50
    property bool canNextPage:false
    property string queryView:style.libraryView || "all"
    property string search:style.libraryView==="search"?style.libraryValue || "":""
    property string selectedPlatform:style.libraryView==="platform"?style.libraryValue || "":""
    property string activeCollectionId:style.libraryView==="collection"?style.libraryValue || "":""
    property var platformChoices:[]
    property var collectionChoices:[]
    property var collectionTarget:({})
    property string collectionCreateType:"manual"
    property string backupPath:""
    property string selectedId:""
    property var detail:null
    property string mode:"browse"
    property int choice:0
    property string message:""
    property var draft:({})
    property string editingId:""
    property string editingCollectionId:""
    property bool importing:false
    property string editionGameId:""
    property var editionChoices:[]
    property int importOffset:0
    property var importItems:[]
    property var steamLibraryChoices:[]
    property string selectedSteamLibrary:""
    property var opticalChoices:[]
    property var pendingUninstalls:[]
    property var pendingUninstall:({})
    property var recoverableManagedInstalls:[]
    property var recoverableManagedInstall:({})
    property string recoveryRuntimeId:""
    property bool recoveringManagedInstall:false
    property var opticalSelectedDisc:({})
    property string opticalObservationId:""
    property string opticalDevice:""
    property bool registeringOpticalGame:false
    property string opticalEntryPath:""
    property string portableRuntimeId:""
    property string portableRuntimeName:""
    property string emulatorId:""
    property var emulatorDraft:({})
    property string firmwareDraft:""
    readonly property var emulators:library?library.emulators:[]
    readonly property var emulatorActions:["Video: "+(emulatorDraft.video || "automatic"),"Controller: "+(emulatorDraft.controller==="xbox-wii-classic"?"Xbox · Wii Classic":"Xbox · Wii Remote + Nunchuk / standard pad"), "Import PS2 firmware: "+(firmwareDraft || (emulatorDraft.firmware?"Saved private copy":"None selected")),"Save setup","Cancel · save nothing"]
    property int field:0
    readonly property var runtimes:library?library.runtimes:[]
    readonly property var tools:["Add game","All","Favorites","Recent","Platforms","Search","Previous",canNextPage?"Next":"End","Emulator setup","Import folder","Continue","Collections","Catalog backup"]
    readonly property var importActions:["Import all reviewed games","Cancel · save nothing","Previous preview page","Next preview page"].concat(importItems.map(g=>g.title+" · "+g.target))
    readonly property bool steamDetail:detail && library && library.editions(detail.id).some(e=>e.runtimeId==="steam")
    readonly property var detailActions:detail?["Play",detail.favorite?"Unfavorite":"Favorite",library && library.canUninstallManagedGame(detail.id)?"Uninstall managed package":"Remove from library","Edit / relink source","Add edition / version","Collections","Edit metadata, aliases & summary","Import cover artwork"].concat(detail.artwork?["Remove cover artwork"]:[]).concat(collectionIsManual()?["Move earlier in collection","Move later in collection"]:[]).concat(steamDetail?["Open Steam · account and offline setup"]:[]).concat(["Back"]):[]
    readonly property int columns:style.libraryLayout==="list"?1:Math.max(1,style.libraryColumns || 4)
    readonly property real scale:appearance.typographyScale || 1
    readonly property string foreground:appearance.textColor || "#F5F1F3"
    readonly property string muted:appearance.mutedColor || "#AAA5AF"
    readonly property string focusColor:appearance.focusColor || "#C994A5"
    readonly property string surface:appearance.surfaceColor || "#202126"
    readonly property string steamStatus: {
        if(mode!=="details" || !detail || !library)return ""
        if(!providerStatus || !providerStatus.appId || !library.steamGameContainsAppId(detail.id,providerStatus.appId))
            return steamDetail?"Steam checks account ownership and offline readiness. Open Steam to sign in, install or prepare offline mode.":""
        const phases={starting:"Starting",process_observed:"Tagged process observed",ended:"Tagged process disappeared",canceled:"Canceled",failed:"Failed"}
        const installation={unknown:"unknown",verified:"manifest and folder checked",unavailable:"check failed"}
        const client={unknown:"unknown",available:"executable present",unavailable:"unavailable"}
        const account={unknown:"unknown",signed_in:"signed in",signed_out:"signed out"}
        const ownership={unknown:"unknown",owned:"reported owned",not_owned:"reported not owned"}
        const connection={unknown:"unknown",online:"reported online",offline:"reported offline"}
        const offline={unknown:"unknown",ready:"reported ready",unavailable:"reported unavailable"}
        const exit={unknown:"unknown",exited:"reported normal",crashed:"reported crash"}
        const local={unknown:"unknown",flushed:"reported flushed",failed:"reported failed"}
        const cloud={unknown:"unknown",synced:"reported synced",pending:"reported pending",failed:"reported failed"}
        const sessionEnd={unknown:"unconfirmed",normal:"reported normal process exit",canceled:"canceled",incomplete:"incomplete · check Steam and saves",recovery_required:"recovery required · check Steam and saves"}
        return "Steam App "+providerStatus.appId+" · last Play: "+(phases[providerStatus.phase] || "unknown")+
            "\nInstall: "+(installation[providerStatus.installation] || "unknown")+" · Client: "+(client[providerStatus.client] || "unknown")+
            "\nAccount: "+(account[providerStatus.account] || "unknown")+" · Ownership: "+(ownership[providerStatus.ownership] || "unknown")+" · Connection: "+(connection[providerStatus.connection] || "unknown")+" · Offline readiness: "+(offline[providerStatus.offline] || "unknown")+
            "\nSession end: "+(sessionEnd[providerStatus.session_end] || "unconfirmed")+" · Exit: "+(exit[providerStatus.exit] || "unknown")+
            "\nLocal save: "+(local[providerStatus.local_save] || "unknown")+" · Cloud: "+(cloud[providerStatus.cloud_save] || "unknown")+
            "\nA normal process exit, cancellation, or wait period does not prove that saves were written or Cloud sync completed."
    }
    signal engage()
    Component.onCompleted:reload()
    onLibraryChanged:reload()
    onQueryViewChanged:{offset=0;reload()}
    onSearchChanged:{offset=0;reload()}
    Connections {target:root.library;function onChanged(){root.reload()}}
    Connections {target:root.library;function onOpticalDiscsChanged(discs,change){
        root.opticalChoices=discs
        if(root.mode==="browse")root.message=change==="inserted"?"A PC optical disc is ready to review. It will not auto-launch. Open Games catalog tools to inspect it.":change==="removed"?"A PC optical disc was removed. Existing catalog entries were preserved.":"PC optical media changed. Review it before any action."
        else if(root.mode==="opticalChooser"){root.choice=0;root.message="Mounted optical sources updated. Review the current media; nothing starts automatically."}
    }}
    Connections {target:root.library;function onSetupChanged(){if(root.mode==="emulatorInstallStatus")root.message=root.library.setupStatus}}
    Connections {target:root.library;function onPortableInstallChanged(){
        if(root.mode==="uninstalling" && !root.library.portableInstallBusy){root.mode="browse";root.message=root.library.portableInstallStatus;root.detail=null;root.reload()}
        else if(root.mode==="windowsInspecting" && !root.library.portableInstallBusy){root.mode="compatibilityStatus";root.message=root.library.portableInstallStatus}
        else if(root.mode==="windowsInspecting"){root.message=root.library.portableInstallStatus}
        else if(root.mode==="installerPreflight" && root.library.portableInstallPreview.schemaVersion){root.mode="installerReview";root.message=root.library.portableInstallStatus}
        else if(root.mode==="installerPreflight" && !root.library.portableInstallBusy){root.message=root.library.portableInstallStatus;root.mode=root.opticalObservationId?"opticalDetails":"catalogTools";root.choice=root.opticalObservationId?1:(root.recoveringManagedInstall?8:4);root.recoveringManagedInstall=false}
        else if(root.mode==="installerInstalling" && !root.library.portableInstallBusy){root.mode="browse";root.message=root.library.portableInstallStatus;root.reload()}
    }}
    Connections {target:root.library;function onImportChanged(){
        if(root.mode!=="scan" && root.mode!=="importPreview")return
        root.importItems=root.library.importPage(root.importOffset)
        if(root.library.importReady){root.mode="importPreview";root.message=root.library.importStatus || root.library.importCount+" files found. Review filenames before importing; no metadata matching is assumed."}
        else if(!root.library.scanning){root.mode="browse";root.message=root.library.errorString || "Import canceled. Nothing was saved."}
    }}
    Connections {
        target:root.dispatcher
        function onRejected(reason){if(root.engaged || root.mode==="details")root.message=reason}
    }
    function reload(){
        if(!library){items=[];canNextPage=false;return}
        const remembered=selectedId
        items=queryView==="collection"?library.pageCollection(activeCollectionId,offset):library.page(queryView,search,offset,selectedPlatform)
        canNextPage=items.length===pageSize && offset+pageSize<catalogLimit
            && (queryView==="collection"
                ? library.pageCollection(activeCollectionId,offset+pageSize).length>0
                : library.page(queryView,search,offset+pageSize,selectedPlatform).length>0)
        if(remembered){const at=items.findIndex(g=>g.id===remembered);if(at>=0)selected=at;else selected=Math.min(selected,items.length-1)}
        else selected=Math.min(selected,items.length-1)
        if(detail){const updated=library.details(detail.id);if(updated.id)detail=updated;else{detail=null;if(mode!=="uninstalling")mode="browse"}}
    }
    function collectionIsManual(){
        if(queryView!=="collection" || !activeCollectionId || !library)return false
        const collection=library.collections().find(c=>c.id===activeCollectionId)
        return !!collection && collection.kind==="manual"
    }
    function finishCollectionEdit(name,filterType,filterValue){
        if(editingCollectionId){
            if(!library.updateCollection(editingCollectionId,name,filterType,filterValue)){message=library.errorString;mode="collectionActions";return}
            editingCollectionId="";collectionChoices=library.collections();mode="collectionChooser";choice=0
            message="Collection settings updated. Game files and saves were not changed.";return
        }
        const id=library.createCollection(name,collectionCreateType==="manual"?"manual":"smart",filterType,filterValue)
        if(!id){message=library.errorString;mode="collectionChooser";return}
        collectionChoices=library.collections();activeCollectionId=id;queryView="collection";offset=0;mode="browse";reload()
    }
    function openDetails(){if(!items[selected])return;selectedId=items[selected].id;detail=library.details(selectedId);choice=0;message="";mode="details"}
    function acquisitionSummary(){
        const record=detail?detail.acquisitionProvenance||{}:{}
        if(!record.recorded)return "\nAcquisition record: Not recorded (legacy catalog entry)."
        return "\nImmutable catalog record: "+record.sourceKind+" · "+record.origin
            +"\nOriginal source reference: "+(record.sourceReference || "Not available")
            +"\nLocator fingerprint: "+record.sourceFingerprint
            +"\nThis records catalog history only; it does not prove license or ownership."
    }
    function openSteam(){
        if(!dispatcher){message="Steam setup is unavailable. Install or repair Steam in the Store.";return}
        if(!dispatcher.dispatch({schemaVersion:1,type:"launch-app",appId:"steam"},originId))
            message="Steam could not open. Install or repair it in the Store, then retry."
    }
    function beginAdd(asImport){
        if(!library || !library.ready){message=library?library.errorString:"Games is unavailable in this profile.";return}
        if(!runtimes.length){message="No compatible game runtime is installed. Game runtimes must be provisioned before a game can be added.";return}
        importing=!!asImport;editingId="";editionGameId="";draft=({schemaVersion:1,title:"",platform:"PC",runtimeId:"",root:"",target:""});choice=0;mode="runtime";message="Choose the installed game or runtime. Nothing is saved until review is confirmed."
    }
    function beginField(index){field=index;mode="keyboard";keyboard.selectedIndex=0;keyboard.value=index===4?search:index===0?draft.title:index===1?draft.platform:index===2?draft.root:index===3?draft.target:index===6?draft.franchise:index===7?draft.tags:index===13?draft.summary:index===14?draft.aliases:""}
    function activateTool(){
        message=""
        const tool=tools[toolbar]
        if(tool==="Add game"){beginAdd();return}
        if(tool==="Emulator setup"){mode="emulators";choice=0;var coverage=library?library.emulatorCoverage:{};var target=coverage.target || ({});var budgets=coverage.budgets || ({});var budgetText=budgets.catalogEntries?"Catalog budgets: "+budgets.catalogEntries+" games, "+budgets.pageEntries+" per page, "+budgets.platforms+" platforms, "+budgets.collections+" collections, "+budgets.editionsPerGame+" editions per game, "+Math.round(budgets.catalogArchiveBytes/1048576)+" MiB per archive, "+Math.round(budgets.artworkRestoreBytes/1048576)+" MiB artwork restore, "+Math.round(budgets.artworkStoreBytes/1048576)+" MiB artwork store.":"Catalog budget contract unavailable.";message="Integrated: "+(coverage.integratedPlatforms || []).join(", ")+". Not integrated: "+(coverage.notIntegrated || []).join(", ")+". Target: "+(target.os || "unknown")+" "+(target.version || "")+" "+(target.architectures || []).join(", ")+". "+budgetText+" Runtime availability does not guarantee game compatibility, controller mapping or performance. Settings and saves are profile-private; games and firmware are never downloaded.";return}
        if(tool==="Import folder"){beginAdd(true);return}
        if(tool==="Search"){beginField(4);return}
        if(tool==="Previous"){offset=Math.max(0,offset-50);selectedId="";selected=-1;reload();return}
        if(tool==="End")return
        if(tool==="Next"){if(canNextPage){offset+=pageSize;selectedId="";selected=-1;reload()}return}
        if(tool==="Platforms"){platformChoices=library.platforms();mode="platformChooser";choice=0;message="Filter this view by a platform. Categories come from games in this profile.";return}
        if(tool==="Collections"){collectionChoices=library.collections();mode="collectionChooser";choice=0;message="Choose a collection, or create a bounded manual/smart collection.";return}
        if(tool==="Catalog backup"){mode="catalogTools";choice=0;message="Export or restore only this profile's Games catalog. Game files, provider data and saves are never copied.";return}
        search="";offset=0;selectedId="";selected=-1;selectedPlatform="";activeCollectionId=""
        queryView=tool==="Favorites"?"favorites":tool==="Recent"?"recent":tool==="Continue"?"continue":"all";reload()
    }
    function activateChoice(){
        if(mode==="emulators"){
            if(choice===emulators.length){mode=library.setupBusy?"emulatorInstallStatus":"emulatorInstallReview";choice=0;message=library.setupBusy?library.setupStatus:"Install the pinned runtimes and cores for the declared Ubuntu 26.04 amd64 integration matrix? This downloads runtime software and uses appliance storage. No game, account or firmware is included. Existing saves are preserved; installation does not certify game compatibility.";return}
            const emulator=emulators[choice];if(!emulator)return
            emulatorId=emulator.id;emulatorDraft=library.emulatorSettings(emulatorId);firmwareDraft=""
            if(!emulatorDraft.schemaVersion){message=library.errorString;return}
            mode="emulatorSettings";choice=4;message=emulator.name+" · "+(emulator.installed?"Runtime installed":"Runtime installation required")+" · "+(emulator.registered?"Available for Games":"Use Install / repair emulators to make this runtime available")+". Integration status is not a gameplay or physical-controller claim."+(emulator.firmwareRequired?" Import your own compatible firmware; Isolde will store a private copy and never download it.":"")
            return
        }
        if(mode==="emulatorInstallReview"){
            if(choice===0){mode="browse";return}
            if(!library.installEmulators(true)){message=library.errorString;return}
            mode="emulatorInstallStatus";choice=0;message=library.setupStatus;return
        }
        if(mode==="emulatorInstallStatus"){
            if(choice===1 && library.setupBusy)library.cancelEmulatorSetup()
            else mode="browse"
            return
        }
        if(mode==="emulatorSettings"){
            if(choice===0){emulatorDraft=Object.assign({},emulatorDraft,{video:emulatorDraft.video==="automatic"?"software":"automatic"});return}
            if(choice===1){
                if(emulatorId==="dolphin")emulatorDraft=Object.assign({},emulatorDraft,{controller:emulatorDraft.controller==="xbox"?"xbox-wii-classic":"xbox"})
                message="Xbox buttons and sticks use SDL mappings. Dolphin offers Wii Remote + Nunchuk or Classic; GameCube keeps its own pad mapping. Wii motion-dependent games require individual compatibility checks.";return
            }
            if(choice===2){
                const emulator=emulators.find(e=>e.id===emulatorId)
                if(!emulator || !emulator.firmwareRequired){message="This runtime needs no firmware import for the listed cartridge/disc routes.";return}
                field=5;mode="keyboard";keyboard.selectedIndex=0;keyboard.value=firmwareDraft;return
            }
            if(choice===3){
                if(!library.configureEmulator(emulatorId,{video:emulatorDraft.video,controller:emulatorDraft.controller},firmwareDraft)){message=library.errorString;return}
                message="Emulator setup saved for this profile. Game files and existing saves are preserved."
            }
            mode="browse";return
        }
        if(mode==="editions"){
            if(choice===editionChoices.length){mode="details";choice=0;return}
            const edition=editionChoices[choice];if(edition)launchEdition(edition.id);return
        }
        if(mode==="platformChooser"){
            if(choice===platformChoices.length){selectedPlatform="";queryView="all"}
            else{selectedPlatform=platformChoices[choice];queryView="platform"}
            search="";offset=0;selectedId="";selected=-1;mode="browse";reload();return
        }
        if(mode==="collectionChooser"){
            if(choice<collectionChoices.length){
                collectionTarget=collectionChoices[choice];mode="collectionActions";choice=0;return
            }
            const createIndex=choice-collectionChoices.length
            if(createIndex<0 || createIndex>6)return
            collectionCreateType=["manual","title","favorite","platform","franchise","tag","recent"][createIndex]
            field=8;mode="keyboard";keyboard.selectedIndex=0;keyboard.value="";return
        }
        if(mode==="collectionActions"){
            if(choice===0){activeCollectionId=collectionTarget.id;queryView="collection";search="";offset=0;selectedId="";selected=-1;mode="browse";reload()}
            else if(choice===1){
                if(!library.removeCollection(collectionTarget.id)){message=library.errorString;return}
                if(activeCollectionId===collectionTarget.id){activeCollectionId="";queryView="all";offset=0}
                collectionChoices=library.collections();mode="collectionChooser";choice=0;message="Collection removed. Games and saves were not changed."
            }else if(choice===2){
                editingCollectionId=collectionTarget.id
                draft={collectionName:collectionTarget.name,kind:collectionTarget.kind,filterType:collectionTarget.filterType,filterValue:collectionTarget.filterValue}
                field=11;mode="keyboard";keyboard.selectedIndex=0;keyboard.value=collectionTarget.name
            }else mode="collectionChooser"
            return
        }
        if(mode==="catalogTools"){
            if(choice===9){mode="browse";return}
            if(choice===7){openSteam();return}
            if(choice===6){const status=library.compatibilityInstallerStatus();mode="compatibilityStatus";choice=0;message="Windows installer support: preflight only. System Wine candidate: "+status.state+". "+status.requirements;return}
            if(choice===5){pendingUninstalls=library.pendingUninstallRecovery();mode="uninstallRecovery";choice=0;message=pendingUninstalls.length?"Select an orphaned uninstall to review. No files are removed.":"No orphaned pending uninstall was found.";return}
            if(choice===4){registeringOpticalGame=false;opticalObservationId="";opticalDevice="";mode="installerRuntime";choice=0;message="Install a standalone x86-64 Linux ELF or an inert Isolde ZIP, USTAR, gzip- or bzip2-compressed USTAR bundle. Only the standalone file is copied; adjacent files and dependencies are not installed. Setup code is never run.";return}
            if(choice===3){opticalChoices=library.mountedOpticalDiscs();mode="opticalChooser";choice=0;message=opticalChoices.length?"Read-only mounted media and drives reporting data media are listed. No disc content is launched or installed automatically.":"No mounted or unmounted PC optical data media is available.";return}
            if(choice===2){steamLibraryChoices=library.steamLibraries();mode="steamLibraries";choice=0;message="Approve only Steam library folders on mounted storage. Each approved library is visible to Steam read-only for this profile.";return}
            if(choice===8){mode="managedRecoveryRuntime";choice=0;message="Choose an installed file runtime. Isolde verifies each private managed install's complete inventory before restoring a catalog entry.";return}
            field=choice===0?9:10;mode="keyboard";keyboard.selectedIndex=0;keyboard.value="";return
        }
        if(mode==="compatibilityStatus"){
            if(choice===0){field=19;mode="keyboard";keyboard.selectedIndex=0;keyboard.value="";return}
            mode="catalogTools";choice=6;return
        }
        if(mode==="windowsInspecting"){library.cancelPortableInstall();mode="compatibilityStatus";choice=0;message=library.portableInstallStatus;return}
        if(mode==="uninstallRecovery"){
            if(choice>=pendingUninstalls.length){mode="catalogTools";choice=5;return}
            pendingUninstall=pendingUninstalls[choice];mode="uninstallRecoveryReview";choice=0
            message="Restore the remaining files or finish removing verified managed files from "+pendingUninstall.path+"? Cleanup may already have removed files. Modified or unowned files and profile saves are preserved. "+(pendingUninstall.conflict?"An existing install blocks both actions.":"No catalog entry will be added.")
            return
        }
        if(mode==="uninstallRecoveryReview"){
            if(choice===1){
                const ok=library.recoverOrphanedUninstall(pendingUninstall.id,true)
                message=ok?"Remaining files restored. No catalog entry was added; inspect the folder before use.":library.errorString
                pendingUninstalls=library.pendingUninstallRecovery();mode="uninstallRecovery";choice=0
            }else if(choice===2){
                if(!library.resumeOrphanedUninstall(pendingUninstall.id,true)){message=library.errorString;return}
                mode="uninstalling";message=library.portableInstallStatus
            }else{mode="uninstallRecovery";choice=0}
            return
        }
        if(mode==="managedRecoveryRuntime"){
            const runtime=runtimes.filter(r=>r.file)[choice];if(!runtime){message="Choose an installed file-based game runtime.";return}
            recoveryRuntimeId=runtime.id;recoverableManagedInstalls=library.unregisteredManagedInstallRecovery()
            mode="managedRecoveryList";choice=0
            message=recoverableManagedInstalls.length?"Select an unregistered private managed install to verify. No catalog entry or files have changed.":"No unregistered managed install was found."
            return
        }
        if(mode==="managedRecoveryList"){
            if(choice>=recoverableManagedInstalls.length){mode="catalogTools";choice=8;return}
            recoverableManagedInstall=recoverableManagedInstalls[choice];mode="managedRecoveryReview";choice=0
            message="Verify the complete installation at "+recoverableManagedInstall.path+" and restore its Games entry? No game files or saves will be changed."
            return
        }
        if(mode==="managedRecoveryReview"){
            if(choice===1){
                if(!library.recoverUnregisteredManagedInstall(recoverableManagedInstall.id,recoveryRuntimeId,true)){message=library.errorString;return}
                recoveringManagedInstall=true;mode="installerPreflight";message=library.portableInstallStatus
            }else{mode="managedRecoveryList";choice=0}
            return
        }
        if(mode==="opticalChooser"){
            if(choice<opticalChoices.length){const disc=opticalChoices[choice];opticalSelectedDisc=disc;opticalObservationId=disc.observationId;opticalDevice=disc.device;mode="opticalDetails";choice=0;message=(disc.volumeLabel || "Unlabeled optical disc")+" · "+disc.filesystem+"\nDevice: "+disc.device+"\n"+(disc.state==="mounted"?"Mounted at: "+disc.mountRoot:"Media is present but not mounted. Replacement detection is limited; verify the disc before mounting or ejecting.")+"\nFull-disc verification: "+(disc.hashState || "unavailable")+"\nMedia identity: "+(disc.mediaId || "not ready")+"\n"+(disc.mediaSetId?"Disc "+disc.discNumber+" of "+disc.discCount+" in media set "+disc.mediaSetId:"No identified disc set")+"\nOnly read-only, no-autorun operations are offered. Content identity is not proof of ownership. Hashing may take several minutes; refresh to retry unreadable media.";return}
            if(choice===opticalChoices.length){opticalChoices=library.refreshOpticalDiscs();choice=0;message=opticalChoices.length?"Optical media list refreshed.":"No mounted or unmounted PC optical data media is available.";return}
            mode="catalogTools";choice=3;return
        }
        if(mode==="opticalDetails"){
            if(opticalSelectedDisc.state==="unmounted"){
                if(choice===0){
                    const ok=library.mountOpticalDisc(opticalObservationId,opticalDevice)
                    opticalChoices=library.mountedOpticalDiscs()
                    mode="opticalChooser";choice=0
                    message=ok?"Read-only mount requested and verified. Select the mounted disc to inspect it; nothing was launched.":library.errorString
                }else if(choice===1){mode="opticalEjectReview";choice=0;message="Isolde will request a normal UDisks eject. Busy media is never force-ejected."}
                else{mode="opticalChooser";choice=0}
                return
            }
            if(choice===0){
                const status=library.revalidateOpticalDisc(opticalObservationId,opticalDevice)
                if(status.state!=="ready"){message=status.state==="replaced"?"The drive now reports different media. Refresh and review it again.":"The selected disc is no longer mounted. Insert it and refresh.";return}
                if(!status.disc.directPlaySupported){message="Full-disc verification is not ready. Wait or return to the disc list and refresh to retry; nothing will launch automatically.";return}
                registeringOpticalGame=true;mode="installerRuntime";choice=0;message="Choose an installed native file runtime. Registration requires a relative executable path and title. Only directly executable Linux ELF files are supported; scripts/installers are never autorun.";return
            }
            if(choice===1){
                if(library.revalidateOpticalDisc(opticalObservationId,opticalDevice).state!=="ready"){message="The selected disc is no longer mounted. Refresh and review it again.";return}
                registeringOpticalGame=false;
                mode="installerRuntime";choice=0;message="Choose an installed file runtime, then select a standalone x86-64 Linux ELF or inert Isolde ZIP, USTAR, gzip- or bzip2-compressed USTAR bundle on this read-only disc. Only the standalone file is copied; adjacent files and dependencies are not installed. Setup programs will not run.";return
            }
            if(choice===2){const ok=library.approveOpticalMedia(opticalObservationId,opticalDevice);message=ok?"This profile approved the mounted disc's content identity. The grant does not transfer to other profiles.":library.errorString;return}
            if(choice===3){const ok=library.revokeOpticalMedia(opticalObservationId,opticalDevice);message=ok?"This profile's disc access was revoked. Catalog records and files remain unchanged.":library.errorString;return}
            if(choice===4){mode="opticalEjectReview";choice=0;message="Isolde will request a normal UDisks unmount and eject. No force eject is attempted; a busy drive will refuse and can be retried after closing its user.";return}
            mode="opticalChooser";choice=0;return
        }
        if(mode==="opticalEjectReview"){
            if(choice===1){const ok=library.ejectOpticalDisc(opticalObservationId,opticalDevice);message=ok?"Safe eject requested. Refresh to confirm the media was removed.":library.errorString}
            mode="opticalDetails";choice=0;return
        }
        if(mode==="installerRuntime"){
            const runtime=runtimes[choice];if(!runtime || !runtime.file){message="Choose an installed file-based game runtime.";return}
            portableRuntimeId=runtime.id;portableRuntimeName=runtime.name;field=17;mode="keyboard";keyboard.selectedIndex=0;keyboard.value=""
            message=registeringOpticalGame?"Enter a relative path to an executable file on the selected disc.":"Select an authorized standalone x86-64 Linux ELF or Isolde ZIP, USTAR, gzip- or bzip2-compressed USTAR bundle. Nothing is run during inspection.";return
        }
        if(mode==="installerReview"){
            if(choice===0){library.cancelPortableInstall();mode=opticalObservationId?"opticalDetails":"catalogTools";choice=opticalObservationId?0:4;return}
            if(!library.confirmPortableInstall()){message=library.errorString;return}
            mode="installerInstalling";message=library.portableInstallStatus;return
        }
        if(mode==="installerInstalling"){if(choice===1){library.cancelPortableInstall();mode="browse";message=library.portableInstallStatus}return}
        if(mode==="installerPreflight"){library.cancelPortableInstall();mode=recoveringManagedInstall?"catalogTools":"browse";choice=recoveringManagedInstall?8:0;message=library.portableInstallStatus;recoveringManagedInstall=false;return}
        if(mode==="steamLibraries"){
            if(choice<steamLibraryChoices.length){selectedSteamLibrary=steamLibraryChoices[choice];mode="steamLibraryActions";choice=0;return}
            if(choice===steamLibraryChoices.length){field=16;mode="keyboard";keyboard.selectedIndex=0;keyboard.value="";return}
            mode="catalogTools";choice=2;return
        }
        if(mode==="steamLibraryActions"){
            if(choice===0){mode="steamLibraryRemoveReview";choice=0;message="Revoking access prevents this profile's Steam session from seeing the external library. Installed game records are kept.";return}
            mode="steamLibraries";choice=0;return
        }
        if(mode==="steamLibraryRemoveReview"){
            if(choice===1){
                if(!library.removeSteamLibrary(selectedSteamLibrary)){message=library.errorString;mode="steamLibraries";return}
                steamLibraryChoices=library.steamLibraries();selectedSteamLibrary="";mode="steamLibraries";choice=0;message="Steam library access revoked for this profile. Game files and catalog entries were not changed.";return
            }
            mode="steamLibraryActions";choice=0;return
        }
        if(mode==="restoreReview"){
            if(choice===1){
                if(!library.restoreCatalog(backupPath,true)){message=library.errorString;mode="browse";return}
                queryView="all";search="";selectedPlatform="";activeCollectionId="";offset=0;selected=-1;selectedId="";detail=null;mode="browse";message="Games catalog restored. Game files, saves and provider records were not changed.";reload()
            }else mode="browse"
            return
        }
        if(mode==="collectionMembers"){
            if(choice>=collectionChoices.length){mode="details";choice=0;return}
            const collection=collectionChoices[choice]
            if(!library.setCollectionMember(collection.id,detail.id,!library.collectionContains(collection.id,detail.id))){message=library.errorString;return}
            message="Collection membership updated · "+collection.name;mode="details";choice=5;return
        }
        if(mode==="scan"){library.cancelImport();mode="browse";return}
        if(mode==="importPreview"){
            if(choice===0){if(!library.commitImport()){message=library.errorString;return}mode="browse";reload();return}
            if(choice===1){library.cancelImport();mode="browse";return}
            if(choice===2)importOffset=Math.max(0,importOffset-50)
            else if(choice===3 && importOffset+50<library.importCount)importOffset+=50
            importItems=library.importPage(importOffset);return
        }
        if(mode==="runtime"){
            const runtime=runtimes[choice];if(!runtime)return
            if(runtime.provider==="steam"){
                if(editionGameId){message="Import Steam games using Add game first. Steam edition association is not available here.";return}
                importOffset=0;choice=1;mode="scan";message="Reading this profile's installed Steam games. Nothing is saved until you confirm the preview."
                if(!library.beginSteamImport()){mode="runtime";message=library.errorString}
                return
            }
            if(importing && !runtime.file){message="Choose a file-based game runtime for folder import.";return}
            draft=Object.assign({},draft,{runtimeId:runtime.id},runtime.file?{}:{root:"",target:""});beginField(importing?1:0);return
        }
        if(mode==="review"){
            if(choice===1){mode="browse";return}
            const id=editionGameId?(library.addEdition(editionGameId,draft)?editionGameId:""):editingId?(library.updateGame(editingId,draft)?editingId:""):library.addGame(draft)
            if(!id){message=library.errorString;return}
            queryView="all";search="";offset=0;selectedId=id;selected=0;mode="browse";message="Game added. Select its card for details and Play.";reload();return
        }
        if(mode==="remove"){
            if(choice===1 && detail){
                const id=detail.id
                if(library.canUninstallManagedGame(id)){
                    if(!library.uninstallManagedGame(id,true)){message=library.errorString;return}
                    mode="uninstalling";message=library.portableInstallStatus;return
                }
                if(!library.removeGame(id)){message=library.errorString;return}detail=null;selectedId=""
            }
            mode=detail?"details":"browse";choice=0;return
        }
        if(mode==="details" && detail){
            if(choice===0){
                editionChoices=library.editions(detail.id)
                if(editionChoices.length>1){mode="editions";choice=0;message="Choose the edition to play."}
                else launchEdition("")
            }else if(choice===1){if(!library.setFavorite(detail.id,!detail.favorite))message=library.errorString}
            else if(choice===2){mode="remove";choice=0;message=library.canUninstallManagedGame(detail.id)?"The complete managed-file manifest will be checked before deleting only this Isolde-owned install. Saves and runtime profiles are outside the install folder.":"Remove only this library entry? Game files and saves stay where they are."}
            else if(choice===3){importing=false;editionGameId="";editingId=detail.id;draft=library.registration(editingId);detail=null;message="Edit this game or relink its folder. Review confirms the complete change; Back cancels.";choice=0;mode="runtime"}
            else if(choice===4){const game=detail.id;beginAdd(false);if(mode==="runtime"){editionGameId=game;message="Add a version to this game. Its launch settings remain separate. Confirming the first edition backs up and upgrades this catalog."}}
            else if(choice===5){collectionChoices=library.collections().filter(c=>c.kind==="manual");mode="collectionMembers";choice=0;message="Choose a manual collection to add/remove this game. Smart shelves are read-only rules."}
            else if(choice===6){draft={franchise:detail.franchise || "",tags:(detail.tags || []).join(", "),summary:detail.summary || "",aliases:(detail.aliases || []).join("; ")};beginField(6)}
            else if(choice===7){field=15;mode="keyboard";keyboard.selectedIndex=0;keyboard.value="";return}
            else{
                let index=8
                if(detail.artwork){
                    if(choice===index){
                        if(!library.clearArtwork(detail.id)){message=library.errorString;return}
                        detail=library.details(detail.id);mode="details";choice=7;return
                    }
                    index++
                }
                if(collectionIsManual()){
                    if(choice===index || choice===index+1){
                        if(!library.moveCollectionMember(activeCollectionId,detail.id,choice===index?-1:1))message=library.errorString
                        reload();mode="details";return
                    }
                    index+=2
                }
                if(steamDetail){
                    if(choice===index){openSteam();return}
                    index++
                }
                if(choice===index){mode="browse";detail=null}
            }
        }
    }
    function launchEdition(id){
        if(!dispatcher){message="Game launching is unavailable.";return}
        const action={schemaVersion:1,type:"launch-game",gameId:detail.id}
        if(id)action.editionId=id
        if(!dispatcher.dispatch(action,originId)){if(library.errorString)message=library.errorString}
        else{mode="browse";detail=null}
    }
    function handleCommand(command){
        if(readOnly)return false
        if(mode==="keyboard"){keyboard.handleCommand(command);return true}
        if(command===inputManager.back && mode==="managedRecoveryReview"){mode="managedRecoveryList";choice=0;return true}
        if(command===inputManager.back && mode==="managedRecoveryList"){mode="managedRecoveryRuntime";choice=0;return true}
        if(command===inputManager.back && mode==="managedRecoveryRuntime"){mode="catalogTools";choice=8;return true}
        if(command===inputManager.back){if(mode==="uninstalling")return true;if(mode!=="browse"){if(mode==="scan" || mode==="importPreview")library.cancelImport();if(mode==="windowsInspecting"){library.cancelPortableInstall();mode="compatibilityStatus";choice=0;message=library.portableInstallStatus}else if(mode==="installerPreflight" || mode==="installerReview" || mode==="installerInstalling"){library.cancelPortableInstall();mode=mode==="installerReview"?(opticalObservationId?"opticalDetails":"catalogTools"):"browse";choice=mode==="catalogTools"?4:0}else if(mode==="collectionActions"){mode="collectionChooser";choice=0}else if(mode==="collectionMembers" || mode==="collectionChooser" && detail){mode="details";choice=5}else if(mode==="collectionChooser"){mode="browse";detail=null}else if(mode==="compatibilityStatus"){mode="catalogTools";choice=6}else if(mode==="uninstallRecoveryReview"){mode="uninstallRecovery";choice=0}else if(mode==="uninstallRecovery"){mode="catalogTools";choice=5}else if(mode==="steamLibraryActions"){mode="steamLibraries";choice=0}else if(mode==="steamLibraryRemoveReview"){mode="steamLibraryActions";choice=0}else if(mode==="steamLibraries"){mode="catalogTools";choice=2}else if(mode==="opticalEjectReview"){mode="opticalDetails";choice=2}else if(mode==="opticalDetails"){mode="opticalChooser";choice=0}else if(mode==="opticalChooser"){mode="catalogTools";choice=3}else if(mode==="installerRuntime"){mode=opticalObservationId?"opticalDetails":"catalogTools";choice=opticalObservationId?0:4;registeringOpticalGame=false}else{mode="browse";detail=null}draft=({});return true}return false}
        if(mode!=="browse"){
            const count=mode==="emulators"?emulators.length+1:mode==="emulatorSettings"?emulatorActions.length:mode==="editions"?editionChoices.length+1:mode==="platformChooser"?platformChoices.length+1:mode==="collectionChooser"?collectionChoices.length+7:mode==="collectionMembers"?collectionChoices.length+1:mode==="collectionActions"?4:mode==="catalogTools"?10:mode==="compatibilityStatus"?2:mode==="windowsInspecting"?1:mode==="uninstallRecovery"?pendingUninstalls.length+1:mode==="uninstallRecoveryReview"?3:mode==="managedRecoveryRuntime"?runtimes.filter(r=>r.file).length:mode==="managedRecoveryList"?recoverableManagedInstalls.length+1:mode==="managedRecoveryReview"?2:mode==="restoreReview"?3:mode==="steamLibraries"?steamLibraryChoices.length+2:mode==="steamLibraryActions"?2:mode==="steamLibraryRemoveReview"?2:mode==="opticalChooser"?opticalChoices.length+2:mode==="opticalDetails"?(opticalSelectedDisc.state==="unmounted"?3:6):mode==="opticalEjectReview"?2:mode==="installerRuntime"?runtimes.length:mode==="installerReview"?2:mode==="installerInstalling"?2:mode==="installerPreflight"?1:mode==="uninstalling"?1:mode==="runtime"?runtimes.length:mode==="details"?detailActions.length:mode==="scan"?1:mode==="importPreview"?importActions.length:2
            if(command===inputManager.navigateDown || command===inputManager.navigateRight)choice=Math.min(count-1,choice+1)
            else if(command===inputManager.navigateUp || command===inputManager.navigateLeft)choice=Math.max(0,choice-1)
            else if(command===inputManager.confirm)activateChoice()
            return true
        }
        if(selected<0){
            if(command===inputManager.navigateUp || command===inputManager.navigateLeft && toolbar===0)return false
            if(command===inputManager.navigateLeft)toolbar=Math.max(0,toolbar-1)
            else if(command===inputManager.navigateRight)toolbar=Math.min(tools.length-1,toolbar+1)
            else if(command===inputManager.navigateDown && items.length){selected=0;selectedId=items[0].id}
            else if(command===inputManager.confirm)activateTool()
            return true
        }
        if(command===inputManager.confirm || command===inputManager.menu){openDetails();return true}
        if(command===inputManager.navigateUp)selected=selected<columns?-1:selected-columns
        else if(command===inputManager.navigateDown){if(selected+columns>=items.length)return false;selected+=columns}
        else if(command===inputManager.navigateLeft){if(selected===0)return false;selected--}
        else if(command===inputManager.navigateRight)selected=Math.min(items.length-1,selected+1)
        if(items[selected])selectedId=items[selected].id
        return true
    }
    Column {
        anchors.fill:parent;spacing:8;visible:root.mode==="browse"
        Row {
            width:parent.width;height:44;spacing:4
            Repeater {model:root.tools
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:(parent.width-4*(root.tools.length-1))/root.tools.length;height:44;radius:root.style.radius || 8;color:root.surface
                    border.color:root.focusColor;border.width:root.engaged && root.selected<0 && root.toolbar===index?2:0
                    Text {anchors.centerIn:parent;textFormat:Text.PlainText;text:modelData;color:root.foreground;font.pixelSize:14*root.scale}
                    MouseArea {anchors.fill:parent;enabled:!root.readOnly;onClicked:{root.engage();root.selected=-1;root.toolbar=index;root.activateTool()}}
                }
            }
        }
        GridView {
            id:grid;width:parent.width;height:Math.max(50,parent.height-100);clip:true
            cellWidth:width/root.columns;cellHeight:root.style.libraryLayout==="list"?78:Math.max(140,root.style.artHeight || 230)
            model:root.items;currentIndex:root.selected;highlightMoveDuration:0
            delegate:Item {
                required property int index;required property var modelData
                width:grid.cellWidth;height:grid.cellHeight
                HomeCard {
                    anchors.fill:parent;anchors.margins:(root.style.cardGap===undefined?16:root.style.cardGap)/2
                    item:({name:modelData.title,logoSource:modelData.artwork || "",presentation:Object.assign({},root.style,{variant:root.style.libraryCardStyle || "media",subtitle:modelData.platform+(modelData.favorite?" · Favorite":"")})})
                    appearance:root.appearance;selected:root.engaged && root.selected===index
                    onActivated:{if(root.readOnly)return;root.engage();root.selected=index;root.openDetails()}
                }
            }
            Text {
                anchors.centerIn:parent;width:parent.width-32;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter
                visible:!grid.count;textFormat:Text.PlainText;text:root.library && root.library.ready?"No games in this view. Choose Add game, or Back to return to the page.":root.library?root.library.errorString:"Games preview is unavailable."
                color:root.muted;font.pixelSize:18*root.scale
            }
        }
        Text {width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.message || "OK details · Menu details · Back leaves library";color:root.muted;font.pixelSize:15*root.scale}
    }
    Rectangle {
        anchors.fill:parent;visible:root.mode!=="browse";color:root.surface;radius:root.style.radius || 12
        Column {
            anchors.fill:parent;anchors.margins:20;spacing:12;visible:root.mode!=="keyboard"
            Text {id:reviewTitle;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.mode==="details" && root.detail?root.detail.title:root.mode==="managedRecoveryRuntime"?"Choose file runtime":root.mode==="managedRecoveryList"?"Recover unregistered managed install":root.mode==="managedRecoveryReview"?"Restore verified Games entry?":root.mode==="runtime"?"Choose game runtime":root.mode==="remove"?"Remove library entry":root.mode==="uninstalling"?"Safe managed uninstall":root.mode==="emulators"?"Emulator setup":root.mode==="emulatorSettings"?"Console settings":root.mode==="emulatorInstallReview"?"Install emulators":root.mode==="emulatorInstallStatus"?"Emulator installation":root.mode==="restoreReview"?"Replace Games catalog?":root.mode==="catalogTools"?"Games catalog backup":root.mode==="compatibilityStatus"?"Compatibility installer requirements":root.mode==="windowsInspecting"?"Inspect Windows executable":root.mode==="uninstallRecovery"?"Recover pending uninstall":root.mode==="uninstallRecoveryReview"?"Restore remaining files?":root.mode==="steamLibraries"?"External Steam libraries":root.mode==="steamLibraryActions"?"Steam library access":root.mode==="steamLibraryRemoveReview"?"Revoke Steam library access?":root.mode==="opticalChooser"?"Mounted PC optical discs":root.mode==="opticalDetails"?"Optical source · review":root.mode==="opticalEjectReview"?"Safely eject optical disc?":root.mode==="installerRuntime"?"Choose portable game runtime":root.mode==="installerReview"?"Review portable game install":root.mode==="installerInstalling"?"Portable game installation":"Review game";color:root.foreground;font.pixelSize:24*root.scale}
            Text {id:reviewSummary;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.mode==="details" && root.detail?(root.detail.platform+" · "+(root.detail.franchise || "No franchise")+"\nCurrent source: "+root.detail.sourceKind+" · Runtime: "+root.detail.runtimeId+"\nCurrent source reference: "+root.detail.sourceReference+"\nThis identifies the current registered locator.\n"+root.acquisitionSummary()+"\n"+((root.detail.aliases || []).length?"Also known as: "+root.detail.aliases.join(", ")+"\n":"")+(root.detail.summary || "")):root.mode==="review"?root.draft.title+" · "+root.draft.platform+"\n"+(root.draft.root?"Allow this game runtime to read: "+root.draft.root+"\nFile: "+root.draft.target:"Installed game: "+root.draft.runtimeId):root.mode==="restoreReview"?"This replaces Games titles, editions, organization and collections for the active profile. It does not delete game files or saves. A private database recovery copy is created first.\n"+root.backupPath:root.mode==="steamLibraryRemoveReview"?"Revoke access to "+root.selectedSteamLibrary+" for this profile only? Other profiles and the external library contents are unchanged.":root.mode==="installerReview"?root.library.portableInstallPreview.title+" · "+root.library.portableInstallPreview.platform+"\nRuntime: "+root.portableRuntimeName+"\nInstall destination: "+root.library.portableInstallPreview.destination+"\nUnpacked size: "+root.library.portableInstallPreview.unpackedBytes+" bytes · "+root.library.portableInstallPreview.archiveEntries+" files\n"+(root.library.portableInstallPreview.archiveFormat==="native-elf"?"Standalone executable: only this file is copied; adjacent files and dependencies are not installed. Its filename becomes the title.":"Bundle contents are copied; only a native ELF entry point is accepted.")+" Nothing runs during install. Network/activation needs cannot be inferred.":root.message;color:root.muted;font.pixelSize:17*root.scale}
            Text {id:steamStatusText;objectName:"steamProviderStatus";width:parent.width;visible:!!root.steamStatus;text:root.steamStatus;textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.muted;font.pixelSize:15*root.scale}
            ListView {
                objectName:"gamesReviewChoices"
                width:parent.width;height:Math.max(0,parent.height-reviewTitle.height-reviewSummary.height-(steamStatusText.visible?steamStatusText.height+parent.spacing:0)-2*parent.spacing);clip:true;currentIndex:root.choice;highlightMoveDuration:0
                model:root.mode==="managedRecoveryRuntime"?root.runtimes.filter(r=>r.file).map(r=>r.name):root.mode==="managedRecoveryList"?root.recoverableManagedInstalls.map(p=>p.id+" · private managed install").concat(["Back"]):root.mode==="managedRecoveryReview"?["Cancel","Verify and restore catalog entry"]:root.mode==="emulatorInstallReview"?["Cancel","Install all listed emulators and cores"]:root.mode==="emulatorInstallStatus"?["Back to Games",root.library && root.library.setupBusy?"Stop installation safely":"Installation finished"]:root.mode==="emulators"?root.emulators.map(e=>e.name+" · "+(e.installed?"Installed":"Not installed")).concat([root.library && root.library.setupBusy?"Installation status":"Install / repair emulators"]):root.mode==="emulatorSettings"?root.emulatorActions:root.mode==="editions"?root.editionChoices.map(e=>e.title+" · "+e.platform).concat(["Back"]):root.mode==="platformChooser"?root.platformChoices.concat(["All platforms"]):root.mode==="collectionChooser"?root.collectionChoices.map(c=>c.name+" · "+c.kind+" · "+c.count).concat(["Create manual collection","Create smart title-match shelf","Create smart Favorites shelf","Create smart platform shelf","Create smart franchise shelf","Create smart tag shelf","Create smart recently-played shelf"]):root.mode==="collectionActions"?["Open collection","Remove collection · keep games and saves","Rename / edit smart rule","Cancel"]:root.mode==="collectionMembers"?root.collectionChoices.map(c=>c.name+(root.library.collectionContains(c.id,root.detail.id)?" · Member · Confirm removes":" · Confirm adds")).concat(["Back"]):root.mode==="catalogTools"?["Export JSON backup","Restore from JSON backup","Manage external Steam access","Mounted PC optical discs · review supported bundles","Install portable Linux bundle","Recover pending uninstall","Windows installer availability","Open Steam · account and offline setup","Recover unregistered managed install","Cancel"]:root.mode==="compatibilityStatus"?["Inspect Windows executable · no install","Back to catalog tools"]:root.mode==="windowsInspecting"?["Cancel inspection"]:root.mode==="uninstallRecovery"?root.pendingUninstalls.map(p=>p.id+(p.conflict?" · destination occupied":" · remaining files")).concat(["Back"]):root.mode==="uninstallRecoveryReview"?["Cancel","Restore remaining files · delete nothing","Finish cleanup · remove verified files"]:root.mode==="steamLibraries"?root.steamLibraryChoices.map(path=>"Approved read-only library · "+path).concat(["Approve external Steam library","Done"]):root.mode==="steamLibraryActions"?["Review and revoke access · "+root.selectedSteamLibrary,"Back"]:root.mode==="steamLibraryRemoveReview"?["Keep approval","Revoke access for this profile"]:root.mode==="opticalChooser"?opticalChoices.map(disc=>(disc.volumeLabel || (disc.state==="unmounted"?"Unlabeled media":"Unlabeled"))+" · "+disc.filesystem+" · "+disc.device+(disc.state==="unmounted"?" · needs mount":"")).concat(["Refresh mounted discs","Back"]):root.mode==="opticalDetails"?opticalSelectedDisc.state==="unmounted"?["Mount read-only","Eject safely…","Back to mounted discs"]:["Register direct-play entry","Install supported portable bundle","Approve for this profile","Revoke profile approval","Eject safely…","Back to mounted discs"]:root.mode==="opticalEjectReview"?["Cancel","Unmount and eject safely"]:root.mode==="installerRuntime"?root.runtimes.map(r=>r.name+(r.file?" · native file runtime":" · unavailable for bundles")):root.mode==="installerReview"?["Cancel · install nothing","Install into new private folder"]:root.mode==="installerInstalling"?["Back to Games","Cancel installation safely"]:root.mode==="installerPreflight"?["Cancel preflight"]:root.mode==="uninstalling"?["Checking verified files…"]:root.mode==="restoreReview"?["Cancel","Restore and replace this profile's Games catalog","Back"]:root.mode==="scan"?["Cancel scan · save nothing"]:root.mode==="importPreview"?root.importActions:root.mode==="runtime"?root.runtimes.map(r=>r.name):root.mode==="details"?root.detailActions:root.mode==="remove"?["Cancel",root.detail && root.library && root.library.canUninstallManagedGame(root.detail.id)?"Uninstall · verify owned files and preserve saves":"Remove entry · keep files and saves"]:[root.editionGameId?"Add edition · preserve other versions":root.editingId?"Save changes and approve source":"Add game and approve source","Cancel"]
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:ListView.view.width;height:48;color:root.surface;radius:8
                    border.color:root.focusColor;border.width:root.choice===index?2:0
                    Text {anchors.fill:parent;anchors.margins:10;verticalAlignment:Text.AlignVCenter;textFormat:Text.PlainText;text:modelData;color:root.foreground;font.pixelSize:18*root.scale;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;enabled:!root.readOnly;onClicked:{root.choice=index;root.activateChoice()}}
                }
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;anchors.fill:parent;visible:root.mode==="keyboard";appearance:root.appearance
        hint:root.field===5?"Your absolute PS2 firmware path · Save setup confirms copying it into this profile":root.field===6?"Franchise or series name":root.field===7?"Comma-separated tags · up to 32 (commas separate tags)":root.field===13?"Game summary · up to 4096 characters":root.field===14?"Alternate titles separated by semicolons · up to 32":root.field===15?"Absolute PNG or JPEG artwork path in a profile-approved media folder":root.field===16?"Absolute existing Steam library folder under /media or /mnt · approval grants read-only access to this profile's Steam session":root.field===17?(root.registeringOpticalGame?"Relative executable path on this optical disc (e.g. bin/game)":root.opticalObservationId?"Absolute standalone ELF or Isolde ZIP/USTAR path on the mounted optical disc":"Absolute standalone ELF or Isolde ZIP/USTAR path inside a profile-approved media folder"):root.field===18?"Game title to register from this optical disc":root.field===19?"Absolute Windows executable path in a profile-approved media folder · inspection only":root.field===8?(root.collectionCreateType==="title"?"Collection name · smart rule matches this text in game titles":"Collection name"):root.field===9?"Absolute JSON export path inside a granted media folder":root.field===10?"Absolute JSON backup path inside a granted media folder":root.field===11?"Collection name":root.field===12?"Platform, title, franchise, or tag text matched by the smart collection":""
        onCanceled:{if(root.field===19){root.mode="compatibilityStatus";return}if(root.field===5){root.mode="emulatorSettings";return}if(root.field===16){root.mode="steamLibraries";return}if(root.field===17){root.mode="installerRuntime";return}if(root.field===18){root.field=17;root.mode="keyboard";keyboard.value=root.opticalEntryPath;return}if(root.field===6 || root.field===7 || root.field===13 || root.field===14 || root.field===15){root.mode="details";return}if(root.field===8){root.mode="collectionChooser";return}if(root.field===11 || root.field===12){root.mode=root.editingCollectionId?"collectionActions":"collectionChooser";root.editingCollectionId="";return}if(root.field===9 || root.field===10){root.mode="catalogTools";return}root.draft=({});root.mode="browse"}
        onAccepted:text=>{
            if(root.field===19){
                root.mode="windowsInspecting";root.choice=0
                if(!root.library.inspectWindowsInstaller(text)){root.message=root.library.errorString;root.mode="compatibilityStatus";return}
                root.message=root.library.portableInstallStatus;return
            }
            if(root.field===5){root.firmwareDraft=text;root.mode="emulatorSettings";return}
            if(root.field===16){
                if(!root.library.addSteamLibrary(text)){root.message=root.library.errorString;root.mode="steamLibraries";return}
                root.steamLibraryChoices=root.library.steamLibraries();root.mode="steamLibraries";root.choice=0;root.message="Approved for this profile only. The library is mounted read-only in isolated Steam sessions.";return
            }
            if(root.field===17){
                if(root.registeringOpticalGame){
                    root.opticalEntryPath=text;root.field=18;root.mode="keyboard";root.keyboard.selectedIndex=0;root.keyboard.value=""
                    root.message="Enter a display title for this disc entry.";return
                }
                if(!root.library.beginPortableInstall(text,root.portableRuntimeId,root.opticalObservationId,root.opticalDevice)){root.message=root.library.errorString;root.mode="installerRuntime";return}
                root.mode="installerPreflight";root.message=root.library.portableInstallStatus;return
            }
            if(root.field===18){
                const id=root.library.registerOpticalGame(text,root.portableRuntimeId,root.opticalEntryPath,root.opticalObservationId,root.opticalDevice)
                if(!id){root.message=root.library.errorString;root.mode="opticalDetails";return}
                root.registeringOpticalGame=false;root.mode="opticalDetails";root.choice=0;root.message="Disc entry added to Games as a read-only source. Play requires this exact disc. Multi-disc entries in the same media set become editions.";root.reload();return
            }
            if(root.field===6){root.draft=Object.assign({},root.draft,{franchise:text});root.beginField(7);return}
            if(root.field===7){
                root.draft=Object.assign({},root.draft,{tags:text.split(",").map(tag=>tag.trim()).filter(tag=>tag.length>0)})
                root.beginField(13);return
            }
            if(root.field===13){root.draft=Object.assign({},root.draft,{summary:text});root.beginField(14);return}
            if(root.field===14){
                const aliases=text.split(";").map(alias=>alias.trim()).filter(alias=>alias.length>0)
                if(!root.library.setOrganization(root.detail.id,root.draft.franchise || "",root.draft.tags || [],aliases,root.draft.summary || "")){root.message=root.library.errorString;root.mode="details";return}
                root.detail=root.library.details(root.detail.id);root.mode="details";root.choice=5;return
            }
            if(root.field===15){
                if(!root.library.setArtwork(root.detail.id,text)){root.message=root.library.errorString;root.mode="details";return}
                root.detail=root.library.details(root.detail.id);root.message="Cover artwork was copied into this profile. The source image was not modified.";root.mode="details";root.choice=7;return
            }
            if(root.field===8){
                if(["platform","franchise","tag"].includes(root.collectionCreateType)){
                    root.draft={collectionName:text};root.field=12;keyboard.selectedIndex=0;keyboard.value=""
                    root.message=root.collectionCreateType==="platform"?"Enter a platform name for this smart collection.":root.collectionCreateType==="franchise"?"Enter franchise text matched by this smart collection.":"Enter tag text matched by this smart collection.";return
                }
                const type=root.collectionCreateType==="manual"?"":root.collectionCreateType
                const value=type==="title"?text:""
                root.finishCollectionEdit(text,type,value);return
            }
            if(root.field===9){
                if(!root.library.exportCatalog(text)){root.message=root.library.errorString;root.mode="browse";return}
                root.message="Profile Games backup exported. Provider records, game files and saves were not copied.";root.mode="browse";return
            }
            if(root.field===10){root.backupPath=text;root.mode="restoreReview";root.choice=0;return}
            if(root.field===11){
                root.draft=Object.assign({},root.draft,{collectionName:text})
                if(root.draft.kind==="smart" && !["favorite","recent"].includes(root.draft.filterType)){
                    root.field=12;keyboard.selectedIndex=0;keyboard.value=root.draft.filterValue || ""
                    root.message=root.draft.filterType==="platform"?"Enter the platform match for this smart collection.":root.draft.filterType==="franchise"?"Enter franchise text matched by this smart collection.":root.draft.filterType==="tag"?"Enter tag text matched by this smart collection.":"Enter the title text matched by this smart collection.";return
                }
                root.finishCollectionEdit(text,root.draft.kind==="manual"?"":root.draft.filterType,root.draft.filterValue || "");return
            }
            if(root.field===12){
                if(root.editingCollectionId)root.finishCollectionEdit(root.draft.collectionName,root.draft.filterType,text)
                else root.finishCollectionEdit(root.draft.collectionName,root.collectionCreateType==="platform"?"platform":root.collectionCreateType==="franchise"?"franchise":"tag",text)
                return
            }
            if(root.field===4){root.search=text;root.queryView="search";root.mode="browse";root.reload();return}
            const keys=["title","platform","root","target"];const next=Object.assign({},root.draft);next[keys[root.field]]=text;root.draft=next
            const runtime=root.runtimes.find(r=>r.id===root.draft.runtimeId)
            if(root.field===0)root.beginField(1)
            else if(root.field===1 && runtime && runtime.file)root.beginField(2)
            else if(root.field===2 && root.importing){
                root.importOffset=0;root.choice=1;root.mode="scan";root.message="Scanning names only. Nothing is saved until review is confirmed."
                if(!root.library.beginImport(root.draft.root,root.draft.runtimeId,root.draft.platform)){root.mode="browse";root.message=root.library.errorString}
            }
            else if(root.field===2)root.beginField(3)
            else{root.mode="review";root.choice=0;root.message=""}
        }
    }
}
