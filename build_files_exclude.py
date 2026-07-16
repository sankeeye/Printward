# PlatformIO pre-build middleware for [env:pandatouch]: skip the source files
# listed in the `custom_build_files_exclude` project option (the GFX library's
# unused ESP32LCD8 / ESP32QSPI panel drivers) so they are not compiled.
# https://docs.platformio.org/en/latest/scripting/middlewares.html
Import("env")

custom_build_files_exclude = env.GetProjectOption("custom_build_files_exclude")
print(" ** Custom_ skip build targets** ", custom_build_files_exclude)


def skip_tgt_from_build(env, node):
    # Returning None drops the file from the build.
    return None


for value in custom_build_files_exclude.split(" "):
    env.AddBuildMiddleware(skip_tgt_from_build, value)
