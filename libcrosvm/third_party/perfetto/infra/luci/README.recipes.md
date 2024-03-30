<!--- AUTOGENERATED BY `./recipes.py test train` -->
# Repo documentation for [perfetto]()
## Table of Contents

**[Recipe Modules](#Recipe-Modules)**
  * [macos_sdk](#recipe_modules-macos_sdk) &mdash; The `macos_sdk` module provides safe functions to access a semi-hermetic XCode installation.
  * [windows_sdk](#recipe_modules-windows_sdk)

**[Recipes](#Recipes)**
  * [macos_sdk:examples/full](#recipes-macos_sdk_examples_full)
  * [perfetto](#recipes-perfetto) &mdash; Recipe for building Perfetto.
  * [windows_sdk:examples/full](#recipes-windows_sdk_examples_full)
## Recipe Modules

### *recipe_modules* / [macos\_sdk](/infra/luci/recipe_modules/macos_sdk)

[DEPS](/infra/luci/recipe_modules/macos_sdk/__init__.py#15): [recipe\_engine/cipd][recipe_engine/recipe_modules/cipd], [recipe\_engine/context][recipe_engine/recipe_modules/context], [recipe\_engine/json][recipe_engine/recipe_modules/json], [recipe\_engine/path][recipe_engine/recipe_modules/path], [recipe\_engine/platform][recipe_engine/recipe_modules/platform], [recipe\_engine/step][recipe_engine/recipe_modules/step]

The `macos_sdk` module provides safe functions to access a semi-hermetic
XCode installation.

Available only to Google-run bots.

#### **class [MacOSSDKApi](/infra/luci/recipe_modules/macos_sdk/api.py#24)([RecipeApi][recipe_engine/wkt/RecipeApi]):**

API for using OS X SDK distributed via CIPD.

&emsp; **@contextmanager**<br>&mdash; **def [\_\_call\_\_](/infra/luci/recipe_modules/macos_sdk/api.py#40)(self):**

Sets up the XCode SDK environment.

This call is a no-op on non-Mac platforms.

This will deploy the helper tool and the XCode.app bundle at
`[START_DIR]/cache/macos_sdk`.

To avoid machines rebuilding these on every run, set up a named cache in
your cr-buildbucket.cfg file like:

    caches: {
      # Cache for mac_toolchain tool and XCode.app
      name: "macos_sdk"
      path: "macos_sdk"
    }

If you have builders which e.g. use a non-current SDK, you can give them
a uniqely named cache:

    caches: {
      # Cache for N-1 version mac_toolchain tool and XCode.app
      name: "macos_sdk_old"
      path: "macos_sdk"
    }

Usage:
  with api.macos_sdk():
    # sdk with mac build bits

Raises:
    StepFailure or InfraFailure.

&emsp; **@property**<br>&mdash; **def [sdk\_dir](/infra/luci/recipe_modules/macos_sdk/api.py#35)(self):**
### *recipe_modules* / [windows\_sdk](/infra/luci/recipe_modules/windows_sdk)

[DEPS](/infra/luci/recipe_modules/windows_sdk/__init__.py#15): [recipe\_engine/cipd][recipe_engine/recipe_modules/cipd], [recipe\_engine/context][recipe_engine/recipe_modules/context], [recipe\_engine/json][recipe_engine/recipe_modules/json], [recipe\_engine/path][recipe_engine/recipe_modules/path], [recipe\_engine/platform][recipe_engine/recipe_modules/platform], [recipe\_engine/step][recipe_engine/recipe_modules/step]

#### **class [WindowsSDKApi](/infra/luci/recipe_modules/windows_sdk/api.py#20)([RecipeApi][recipe_engine/wkt/RecipeApi]):**

API for using Windows SDK distributed via CIPD.

&emsp; **@contextmanager**<br>&mdash; **def [\_\_call\_\_](/infra/luci/recipe_modules/windows_sdk/api.py#29)(self):**

Setups the Windows SDK environment.

This call is a no-op on non-Windows platforms.

Raises:
    StepFailure or InfraFailure.
## Recipes

### *recipes* / [macos\_sdk:examples/full](/infra/luci/recipe_modules/macos_sdk/examples/full.py)

[DEPS](/infra/luci/recipe_modules/macos_sdk/examples/full.py#15): [macos\_sdk](#recipe_modules-macos_sdk), [recipe\_engine/platform][recipe_engine/recipe_modules/platform], [recipe\_engine/properties][recipe_engine/recipe_modules/properties], [recipe\_engine/step][recipe_engine/recipe_modules/step]

&mdash; **def [RunSteps](/infra/luci/recipe_modules/macos_sdk/examples/full.py#23)(api):**
### *recipes* / [perfetto](/infra/luci/recipes/perfetto.py)

[DEPS](/infra/luci/recipes/perfetto.py#18): [depot\_tools/gsutil][depot_tools/recipe_modules/gsutil], [macos\_sdk](#recipe_modules-macos_sdk), [windows\_sdk](#recipe_modules-windows_sdk), [recipe\_engine/buildbucket][recipe_engine/recipe_modules/buildbucket], [recipe\_engine/cipd][recipe_engine/recipe_modules/cipd], [recipe\_engine/context][recipe_engine/recipe_modules/context], [recipe\_engine/file][recipe_engine/recipe_modules/file], [recipe\_engine/path][recipe_engine/recipe_modules/path], [recipe\_engine/platform][recipe_engine/recipe_modules/platform], [recipe\_engine/properties][recipe_engine/recipe_modules/properties], [recipe\_engine/raw\_io][recipe_engine/recipe_modules/raw_io], [recipe\_engine/step][recipe_engine/recipe_modules/step]

Recipe for building Perfetto.

&mdash; **def [BuildForPlatform](/infra/luci/recipes/perfetto.py#138)(api, ctx, platform):**

&mdash; **def [GnArgs](/infra/luci/recipes/perfetto.py#81)(platform):**

&mdash; **def [RunSteps](/infra/luci/recipes/perfetto.py#164)(api, repository):**

&mdash; **def [UploadArtifact](/infra/luci/recipes/perfetto.py#90)(api, ctx, platform, out_dir, artifact):**
### *recipes* / [windows\_sdk:examples/full](/infra/luci/recipe_modules/windows_sdk/examples/full.py)

[DEPS](/infra/luci/recipe_modules/windows_sdk/examples/full.py#15): [windows\_sdk](#recipe_modules-windows_sdk), [recipe\_engine/platform][recipe_engine/recipe_modules/platform], [recipe\_engine/properties][recipe_engine/recipe_modules/properties], [recipe\_engine/step][recipe_engine/recipe_modules/step]

&mdash; **def [RunSteps](/infra/luci/recipe_modules/windows_sdk/examples/full.py#23)(api):**

[depot_tools/recipe_modules/gsutil]: https://chromium.googlesource.com/chromium/tools/depot_tools.git/+/5345b34aaf50107d0a07146b9319ef19203f65a0/recipes/README.recipes.md#recipe_modules-gsutil
[recipe_engine/recipe_modules/buildbucket]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-buildbucket
[recipe_engine/recipe_modules/cipd]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-cipd
[recipe_engine/recipe_modules/context]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-context
[recipe_engine/recipe_modules/file]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-file
[recipe_engine/recipe_modules/json]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-json
[recipe_engine/recipe_modules/path]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-path
[recipe_engine/recipe_modules/platform]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-platform
[recipe_engine/recipe_modules/properties]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-properties
[recipe_engine/recipe_modules/raw_io]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-raw_io
[recipe_engine/recipe_modules/step]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/README.recipes.md#recipe_modules-step
[recipe_engine/wkt/RecipeApi]: https://chromium.googlesource.com/infra/luci/recipes-py.git/+/2030661a4ff2a6b64b0651f2c44aabed8c71223f/recipe_engine/recipe_api.py#856