function Setup-OpenCV {
    if ( ! ( Test-Path function:Log-Output ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    if ( ! ( Test-Path function:Check-Git ) ) {
        . $PSScriptRoot/Check-Git.ps1
    }

    Check-Git

    if ( ! ( Test-Path function:Ensure-Location ) ) {
        . $PSScriptRoot/Ensure-Location.ps1
    }

    if ( ! ( Test-Path function:Invoke-GitCheckout ) ) {
        . $PSScriptRoot/Invoke-GitCheckout.ps1
    }

    if ( ! ( Test-Path function:Invoke-External ) ) {
        . $PSScriptRoot/Invoke-External.ps1
    }

    Log-Information 'Setting up OpenCV...'

    $OpencvVersion = $BuildSpec.dependencies.'opencv'.version
    $OpencvRepository = $BuildSpec.dependencies.'opencv'.repository
    $OpencvBranch = $BuildSpec.dependencies.'opencv'.branch
    $OpencvHash = $BuildSpec.dependencies.'opencv'.hash

    if ( $OpencvVersion -eq '' ) {
        throw 'No opencv version found in buildspec.json.'
    }

    Push-Location -Stack BuildTemp
    Ensure-Location -Path "$(Resolve-Path -Path "${ProjectRoot}/../")/opencv"

    if ( ! ( ( $script:SkipAll ) -or ( $script:SkipUnpack ) ) ) {
        Invoke-GitCheckout -Uri $OpencvRepository -Commit $OpencvHash -Path . -Branch $OpencvBranch
    }

    if ( ! ( ( $script:SkipAll ) -or ( $script:SkipBuild ) ) ) {

        $NumProcessors = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

        if ( $NumProcessors -gt 1 ) {
            $env:UseMultiToolTask = $true
            $env:EnforceProcessCountAcrossBuilds = $true
        }

        $DepsPath = "plugin-deps-${script:DepsVersion}-qt${script:QtVersion}-${script:Target}"

        $CmakeArgs = @(
            '-G', $CmakeGenerator
            "-DCMAKE_SYSTEM_VERSION=${script:PlatformSDK}"
            "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
            "-DCMAKE_BUILD_TYPE=${script:Configuration}"
            '-DBUILD_PERF_TESTS=OFF'
            '-DBUILD_TESTS=OFF'
            "$( if ( $script:Configuration -eq 'RelWithDebInfo' ) { '-DBUILD_WITH_DEBUG_INFO=ON' })"
            '-DBUILD_opencv_apps=OFF'
            '-DBUILD_opencv_calib3d=OFF'
            '-DBUILD_opencv_dnn=OFF'
            '-DBUILD_opencv_features2d=OFF'
            '-DBUILD_opencv_flann=OFF'
            #'-DBUILD_opencv_highgui=OFF'
            '-DBUILD_opencv_gapi=OFF'
            '-DBUILD_opencv_java_bindings_generator=OFF'
            '-DBUILD_opencv_js=OFF'
            '-DBUILD_opencv_js_bindings_generator=OFF'
            '-DBUILD_opencv_ml=OFF'
            '-DBUILD_opencv_objc_bindings_generator=OFF'
            '-DBUILD_opencv_objdetect=OFF'
            '-DBUILD_opencv_photo=OFF'
            '-DBUILD_opencv_python3=OFF'
            '-DBUILD_opencv_python_bindings_generator=OFF'
            '-DBUILD_opencv_python_tests=OFF'
            '-DBUILD_opencv_stitching=OFF'
            '-DBUILD_opencv_ts=OFF'
            '-DBUILD_opencv_video=OFF'
            '-DBUILD_opencv_videoio=OFF'
            '-DBUILD_opencv_world=OFF'
            "-DCMAKE_INSTALL_PREFIX:PATH=$(Resolve-Path -Path "${ProjectRoot}/../obs-build-dependencies/${DepsPath}")"
            "-DCMAKE_PREFIX_PATH:PATH=$(Resolve-Path -Path "${ProjectRoot}/../obs-build-dependencies/${DepsPath}")"
        )

        Log-Debug "Attempting to configure OpenCV with CMake arguments: $($CmakeArgs | Out-String)"
        Log-Information "Configuring OpenCV..."
        Invoke-External cmake -S . -B plugin_build_${script:Target} @CmakeArgs

        # Log-Information 'Building libobs and obs-frontend-api...'
        Log-Information "--config opencv"
        $CmakeArgs = @(
            #'--config', "$( if ( $script:Configuration -eq '' ) { 'Release' } else { $script:Configuration })"
            '--config', "$( if( $script:Configuration -in "RelWithDebInfo", "MinSizeRel") { 'Release' } else { $script:Configuration })"
            #'--config Release'
        )

        if ( $VerbosePreference -eq 'Continue' ) {
           $CmakeArgs+=('--verbose')
        }
        Log-Information "Building OpenCV..."
        Invoke-External cmake --build plugin_build_${script:Target} @CmakeArgs
        Log-Information "Installing OpenCV..."
        Invoke-External cmake --install plugin_build_${script:Target} @CmakeArgs
    }
    Pop-Location -Stack BuildTemp
}
