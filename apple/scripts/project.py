"""Generate a dependency-free Xcode project for both native input hosts."""
import hashlib
import json
import plistlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def identifier(name):
    return hashlib.sha1(name.encode()).hexdigest()[:24].upper()


def quoted(value):
    return json.dumps(str(value))


def main():
    objects = []
    def add(name, body):
        key = identifier(name)
        objects.append(f'{key} = {{ {body} }};')
        return key
    def sequence(values):
        return '(' + ','.join(values) + (',' if values else '') + ')'
    def settings(values):
        return '{' + ''.join(quoted(k)+' = '+(sequence([quoted(x) for x in v]) if isinstance(v,list) else quoted(v))+';' for k,v in values.items())+'}'
    def configurations(name, values):
        configs=[]
        for mode in ['Debug','Release']:
            current={**values,'SWIFT_OPTIMIZATION_LEVEL':'-Onone' if mode=='Debug' else '-O'}
            configs.append(add(name+mode, 'isa = XCBuildConfiguration; name = '+mode+'; buildSettings = '+settings(current)+';'))
        return add(name+'configs','isa = XCConfigurationList; buildConfigurations = '+sequence(configs)+'; defaultConfigurationName = Release; defaultConfigurationIsVisible = 0;')
    shared=['Shared/IPAEngine.mm','Shared/InputModel.swift','Shared/KeyboardViews.swift','Shared/Updates.swift']
    targets={
        'PhoneticIPA-Mac': {'sources':shared+['macOS/main.swift'],'type':'com.apple.product-type.application','bundle':'org.nephelium.inputmethod.PhoneticIPA','sdk':'macosx','plist':'macOS/Info.plist','product':'PhoneticIPA-Mac.app'},
        'PhoneticIPA-iOS': {'sources':shared+['iOS/PhoneticIPAApp.swift'],'type':'com.apple.product-type.application','bundle':'org.nephelium.phonetic-ipa.ios','sdk':'iphoneos','plist':'iOS/App-Info.plist','product':'PhoneticIPA-iOS.app'},
        'PhoneticIPA-Keyboard': {'sources':shared+['iOS/KeyboardViewController.swift'],'type':'com.apple.product-type.app-extension','bundle':'org.nephelium.phonetic-ipa.ios.keyboard','sdk':'iphoneos','plist':'iOS/Keyboard-Info.plist','product':'PhoneticIPA-Keyboard.appex'},
        'PhoneticIPA-UITests': {'sources':['Tests/KeyboardUITests.swift'],'type':'com.apple.product-type.bundle.ui-testing','bundle':'org.nephelium.phonetic-ipa.uitests','sdk':'iphoneos','plist':'','product':'PhoneticIPA-UITests.xctest'},
    }
    refs={}
    for path in sorted(set(sum([x['sources'] for x in targets.values()],[])) | {'Shared/IPAEngine.h'}):
        kind='sourcecode.swift' if path.endswith('.swift') else 'sourcecode.cpp.objcpp' if path.endswith('.mm') else 'sourcecode.c.h'
        refs[path]=add(path,'isa = PBXFileReference; lastKnownFileType = '+kind+'; path = '+quoted(path)+'; sourceTree = "<group>";')
    resource=add('IPA','isa = PBXFileReference; lastKnownFileType = folder; path = Generated/IPA; sourceTree = "<group>";')
    icons=add('Icons','isa = PBXFileReference; lastKnownFileType = folder.assetcatalog; path = Assets.xcassets; sourceTree = "<group>";')
    productRefs=[]
    for name, spec in targets.items():
        productRefs.append(add(name+'product','isa = PBXFileReference; explicitFileType = '+('wrapper.cfbundle' if 'UITests' in name else 'wrapper.app-extension' if 'Keyboard' in name else 'wrapper.application')+'; path = '+quoted(spec['product'])+'; sourceTree = BUILT_PRODUCTS_DIR;'))
    products=add('products','isa = PBXGroup; name = Products; children = '+sequence(productRefs)+'; sourceTree = "<group>";')
    group=add('group','isa = PBXGroup; children = '+sequence(list(refs.values())+[resource,icons,products])+'; sourceTree = "<group>";')
    targetIDs=[]
    for index,(name,spec) in enumerate(targets.items()):
        sourceBuilds=[add(name+path,'isa = PBXBuildFile; fileRef = '+refs[path]+';') for path in spec['sources']]
        sources=add(name+'sources','isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = '+sequence(sourceBuilds)+'; runOnlyForDeploymentPostprocessing = 0;')
        assets=add(name+'asset','isa = PBXBuildFile; fileRef = '+resource+';')
        resourceBuilds=[] if 'UITests' in name else [assets]
        if spec['type']=='com.apple.product-type.application':
            resourceBuilds.append(add(name+'icons','isa = PBXBuildFile; fileRef = '+icons+';'))
        resources=add(name+'resources','isa = PBXResourcesBuildPhase; buildActionMask = 2147483647; files = '+sequence(resourceBuilds)+'; runOnlyForDeploymentPostprocessing = 0;')
        frameworks=add(name+'frameworks','isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0;')
        phases=[sources,frameworks,resources]
        dependencies=[]
        if name=='PhoneticIPA-iOS':
            embedFile=add('embedfile','isa = PBXBuildFile; fileRef = '+productRefs[2]+'; settings = { ATTRIBUTES = (RemoveHeadersOnCopy,); };')
            phases.append(add('embed','isa = PBXCopyFilesBuildPhase; buildActionMask = 2147483647; dstPath = ""; dstSubfolderSpec = 13; files = '+sequence([embedFile])+'; name = "Embed App Extensions"; runOnlyForDeploymentPostprocessing = 0;'))
            proxy=add('extensionproxy','isa = PBXContainerItemProxy; containerPortal = '+identifier('project')+'; proxyType = 1; remoteGlobalIDString = '+identifier('PhoneticIPA-Keyboardtarget')+'; remoteInfo = "PhoneticIPA-Keyboard";')
            dependencies=[add('extensiondependency','isa = PBXTargetDependency; target = '+identifier('PhoneticIPA-Keyboardtarget')+'; targetProxy = '+proxy+';')]
        if name=='PhoneticIPA-UITests':
            proxy=add('testproxy','isa = PBXContainerItemProxy; containerPortal = '+identifier('project')+'; proxyType = 1; remoteGlobalIDString = '+identifier('PhoneticIPA-iOStarget')+'; remoteInfo = "PhoneticIPA-iOS";')
            dependencies=[add('testdependency','isa = PBXTargetDependency; target = '+identifier('PhoneticIPA-iOStarget')+'; targetProxy = '+proxy+';')]
        values={
            'PRODUCT_NAME':name,'PRODUCT_BUNDLE_IDENTIFIER':spec['bundle'],'SDKROOT':spec['sdk'],
            'SWIFT_VERSION':'5.0','CLANG_ENABLE_MODULES':'YES','CLANG_ENABLE_OBJC_ARC':'YES',
            'CLANG_CXX_LANGUAGE_STANDARD':'c++17','INFOPLIST_FILE':spec['plist'],
            'SWIFT_OBJC_BRIDGING_HEADER':'$(SRCROOT)/Shared/IPAEngine.h',
            'HEADER_SEARCH_PATHS':['$(inherited)','$(IPA_SOURCE_ROOT)'],
            'IPA_SOURCE_ROOT':('$(SRCROOT)/..' if (ROOT.parent/'engine/ipa').is_dir() else '$(SRCROOT)/../msime-windows'),'OTHER_LDFLAGS':['$(inherited)','-lsqlite3'],
            'CODE_SIGN_STYLE':'Automatic','CURRENT_PROJECT_VERSION':'5','MARKETING_VERSION':'0.1.4',
            'GENERATE_INFOPLIST_FILE':'NO','ENABLE_USER_SCRIPT_SANDBOXING':'YES',
        }
        if spec['sdk']=='macosx':
            values.update({'MACOSX_DEPLOYMENT_TARGET':'13.0','OTHER_LDFLAGS':['$(inherited)','-lsqlite3','-framework','InputMethodKit'],'CODE_SIGN_IDENTITY':'-'})
        else:
            values.update({'IPHONEOS_DEPLOYMENT_TARGET':'16.0','TARGETED_DEVICE_FAMILY':'1,2','SUPPORTED_PLATFORMS':'iphoneos iphonesimulator','CODE_SIGN_ENTITLEMENTS':'iOS/Shared.entitlements'})
            if 'Keyboard' in name: values.update({'APPLICATION_EXTENSION_API_ONLY':'YES','SKIP_INSTALL':'YES'})
            if 'UITests' in name:
                values.update({'TEST_TARGET_NAME':'PhoneticIPA-iOS','GENERATE_INFOPLIST_FILE':'YES','SWIFT_OBJC_BRIDGING_HEADER':'','CODE_SIGN_ENTITLEMENTS':'','SKIP_INSTALL':'YES'})
        if spec['type']=='com.apple.product-type.application':
            values['ASSETCATALOG_COMPILER_APPICON_NAME']='AppIcon'
        config=configurations(name,values)
        targetIDs.append(add(name+'target','isa = PBXNativeTarget; buildConfigurationList = '+config+'; buildPhases = '+sequence(phases)+'; buildRules = (); dependencies = ('+','.join(dependencies)+'); name = '+quoted(name)+'; productName = '+quoted(name)+'; productReference = '+productRefs[index]+'; productType = '+quoted(spec['type'])+';'))
    projectConfig=configurations('project',{})
    project=add('project','isa = PBXProject; attributes = { LastUpgradeCheck = 2710; BuildIndependentTargetsInParallel = 1; TargetAttributes = { '+identifier('PhoneticIPA-UITeststarget')+' = { TestTargetID = '+identifier('PhoneticIPA-iOStarget')+'; }; }; }; buildConfigurationList = '+projectConfig+'; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; knownRegions = (en, Base, "zh-Hans"); mainGroup = '+group+'; productRefGroup = '+products+'; projectDirPath = ""; projectRoot = ""; targets = '+sequence(targetIDs)+';')
    folder=ROOT/'PhoneticIPA.xcodeproj'
    folder.mkdir(exist_ok=True)
    (folder/'project.pbxproj').write_text('// !$*UTF8*$!\n{ archiveVersion = 1; classes = {}; objectVersion = 56; objects = {\n'+'\n'.join(objects)+'\n}; rootObject = '+project+'; }\n',encoding='utf-8')
    schemes=folder/'xcshareddata/xcschemes'
    schemes.mkdir(parents=True,exist_ok=True)
    def buildable(name):
        return f'<BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{identifier(name+"target")}" BuildableName="{targets[name]["product"]}" BlueprintName="{name}" ReferencedContainer="container:PhoneticIPA.xcodeproj"/>'
    for name in ['PhoneticIPA-Mac','PhoneticIPA-iOS']:
        testable=f'<TestableReference skipped="NO">{buildable("PhoneticIPA-UITests")}</TestableReference>' if name.endswith('iOS') else ''
        (schemes/(name+'.xcscheme')).write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="2710" version="1.7">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">{buildable(name)}</BuildActionEntry></BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables>{testable}</Testables></TestAction>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB"><BuildableProductRunnable runnableDebuggingMode="0">{buildable(name)}</BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release"/><AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/></Scheme>''',encoding='utf-8')
    base={'CFBundleDevelopmentRegion':'zh-Hans','CFBundleExecutable':'$(EXECUTABLE_NAME)','CFBundleIdentifier':'$(PRODUCT_BUNDLE_IDENTIFIER)','CFBundleInfoDictionaryVersion':'6.0','CFBundleName':'国际音标输入法','CFBundleShortVersionString':'$(MARKETING_VERSION)','CFBundleVersion':'$(CURRENT_PROJECT_VERSION)','CFBundlePackageType':'APPL'}
    mac_id='org.nephelium.inputmethod.PhoneticIPA'
    mode_id=mac_id+'.Hans'
    mode={'TISInputSourceID':mode_id,'TISIntendedLanguage':'zh-Hans','tsInputModeCharacterRepertoireKey':['Hans','Latn'],
          'tsInputModeDefaultStateKey':True,'tsInputModeIsVisibleKey':True,'tsInputModePrimaryInScriptKey':True,
          'tsInputModeScriptKey':'smSimpChinese','tsInputModeMenuIconFileKey':'IPA/MenuIcon.tiff','tsInputModePaletteIconFileKey':'IPA/MenuIcon.tiff'}
    mac={**base,'CFBundleDisplayName':'国际音标输入法','LSBackgroundOnly':True,'NSPrincipalClass':'NSApplication','LSMinimumSystemVersion':'13.0',
         'InputMethodConnectionName':mac_id+'_Connection','InputMethodServerControllerClass':'PhoneticInputController',
         'InputMethodServerDelegateClass':'PhoneticInputController','tsInputMethodCharacterRepertoireKey':['Latn','Hans','Hant'],
         'tsInputMethodIconFileKey':'IPA/MenuIcon.tiff',
         'ComponentInputModeDict':{'tsInputModeListKey':{mode_id:mode},'tsVisibleInputModeOrderedArrayKey':[mode_id]}}
    app={**base,'CFBundleDisplayName':'国际音标输入法','LSRequiresIPhoneOS':True,'UILaunchScreen':{},'UIApplicationSceneManifest':{'UIApplicationSupportsMultipleScenes':False},'UISupportedInterfaceOrientations':['UIInterfaceOrientationPortrait','UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight']}
    keyboard={**base,'CFBundleDisplayName':'国际音标输入法','CFBundlePackageType':'XPC!','NSExtension':{'NSExtensionPointIdentifier':'com.apple.keyboard-service','NSExtensionPrincipalClass':'$(PRODUCT_MODULE_NAME).KeyboardViewController','NSExtensionAttributes':{'IsASCIICapable':True,'PrefersRightToLeft':False,'PrimaryLanguage':'zh-Hans','RequestsOpenAccess':True}}}
    for path,content in [('macOS/Info.plist',mac),('iOS/App-Info.plist',app),('iOS/Keyboard-Info.plist',keyboard),('iOS/Shared.entitlements',{'com.apple.security.application-groups':['group.org.nephelium.phonetic-ipa']})]:
        (ROOT/path).write_bytes(plistlib.dumps(content,sort_keys=False))
    print(folder)


if __name__=='__main__': main()
