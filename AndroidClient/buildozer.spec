[app]
title = Checkers Online
package.name = checkers
package.domain = com.example
source.dir = .
source.include_exts = py,png,jpg,kv,atlas
version = 1.0
requirements = python3==3.10.11,hostpython3==3.10.11,kivy==2.3.0,pyjnius==1.6.1
orientation = portrait
fullscreen = 1
android.permissions = INTERNET
android.api = 33
android.minapi = 24
android.ndk = 25b
android.archs = arm64-v8a
android.accept_sdk_license = yes

[buildozer]
log_level = 2
warn_on_root = 0