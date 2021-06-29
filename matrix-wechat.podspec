#pod lib lint matrix-wechat.podspec --use-libraries
#pod lib lint --verbose --skip-import-validation --allow-warnings matrix-wechat.podspec
#pod spec lint matrix-wechat.podspec --allow-warnings
#pod trunk me 
#pod trunk push matrix-wechat.podspec --allow-warnings
#pod trunk info matrix-wechat

Pod::Spec.new do |matrix|
  matrix.name             = 'matrix-wechat'
  matrix.version          = '1.0.1'
  matrix.summary          = 'Matrix for iOS/macOS is a performance probe tool developed and used by WeChat.'
  matrix.description      = <<-DESC
                            Matrix for iOS/macOS is a performance probe tool developed and used by WeChat. 
                            It is currently integrated into the APM (Application Performance Manage) platform inside WeChat. 
                            The monitoring scope of the current tool includes: crash, lag, and out-of-memory, which includes the following two plugins:

                            1.WCCrashBlockMonitorPlugin: Based on [KSCrash](https://github.com/kstenerud/KSCrash) framework, it features cutting-edge lag stack capture capabilities with crash cpature.

                            2.WCMemoryStatPlugin: A performance-optimized out-of-memory monitoring tool that captures memory allocation and the callstack of an application's out-of-memory event.
                            DESC
  matrix.homepage         = 'https://github.com/Tencent/matrix'
  matrix.license          = { :type => 'BSD', :file => 'LICENSE' }
  matrix.author           = { 'johnzjchen' => 'johnzjchen@tencent.com' }
  matrix.source           = { :git => 'https://github.com/Tencent/matrix.git', :branch => "feature/ios-matrix-cocopods-1.0.1" }
  matrix.module_name      = "Matrix"

  matrix.ios.deployment_target = '10.0'
  matrix.osx.deployment_target = "10.10"
  matrix.libraries        = "z", "c++"
  matrix.frameworks       = "CoreFoundation", "Foundation"
  matrix.ios.frameworks   = "UIKit"
  matrix.osx.frameworks   = "AppKit"

  matrix.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "gnu++1y",
    "CLANG_CXX_LIBRARY" => "libc++"
  }

  non_arc_files_1           = "matrix/matrix-iOS/Matrix/Matrix/Util/MatrixBaseModel.{h,mm}"
  non_arc_files_2           = "matrix/matrix-iOS/Matrix/WCMemoryStat/MemoryLogger/ObjectEvent/nsobject_hook.{h,mm}"

  matrix.subspec 'matrixNonARC1' do |non_arc1|
    non_arc1.requires_arc = false
    non_arc1.source_files = non_arc_files_1
    non_arc1.public_header_files = ["matrix/matrix-iOS/Matrix/Matrix/Util/MatrixBaseModel.h"]
  end

  matrix.subspec 'matrixARC' do |arc|
    arc.requires_arc     = true
    arc.source_files     = "matrix/matrix-iOS/Matrix/**/*.{h,m,mm,c,cpp}"
    arc.exclude_files    = [non_arc_files_1,non_arc_files_2]
    arc.public_header_files = ["matrix/matrix-iOS/Matrix/Matrix/matrix.h", "matrix/matrix-iOS/Matrix/Matrix/MatrixFramework.h" ,
      "matrix/matrix-iOS/Matrix/Matrix/AppReboot/MatrixAppRebootType.h","matrix/matrix-iOS/Matrix/Matrix/Issue/MatrixIssue.h",
      "matrix/matrix-iOS/Matrix/Matrix/Plugin/*.{h}",
      "matrix/matrix-iOS/Matrix/Matrix/Log/MatrixAdapter.h", "matrix/matrix-iOS/Matrix/Matrix/Test/*.{h}",
      "matrix/matrix-iOS/Matrix/WCMemoryStat/MemoryStatPlugin/WCMemoryStatConfig.h",
      "matrix/matrix-iOS/Matrix/WCMemoryStat/MemoryStatPlugin/WCMemoryStatPlugin.h", "matrix/matrix-iOS/Matrix/WCMemoryStat/MemoryStatPlugin/Record/WCMemoryStatModel.h",
      "matrix/matrix-iOS/Matrix/WCMemoryStat/MemoryLogger/memory_stat_err_code.h", "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/WCCrashBlockMonitorPlugin.h",
      "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/WCCrashBlockMonitorPlugin+Upload.h","matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/WCCrashBlockMonitorConfig.h",
      "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/Main/WCCrashBlockMonitorDelegate.h","matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/Main/Utility/WCCrashReportInfoUtil.h",
      "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/Main/Utility/WCCrashReportInterpreter.h","matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/Main/File/WCCrashBlockFileHandler.h",
      "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/Main/BlockMonitor/WCBlockTypeDef.h","matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/CrashBlockPlugin/Main/BlockMonitor/WCBlockMonitorConfiguration.h",
      "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/KSCrash/Recording/KSCrashReportWriter.h", "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/KSCrash/Recording/Tools/KSMachineContext.h",
      "matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/KSCrash/Recording/Tools/KSThread.h","matrix/matrix-iOS/Matrix/WCCrashBlockMonitor/KSCrash/Recording/Tools/KSStackCursor.h"]
    arc.dependency  'matrix-wechat/matrixNonARC1'
  end

  matrix.subspec 'matrixNonARC2' do |non_arc2|
    non_arc2.requires_arc = false
    non_arc2.source_files = non_arc_files_2
    non_arc2.dependency 'matrix-wechat/matrixARC'
  end

end




