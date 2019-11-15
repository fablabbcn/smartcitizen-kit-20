# Smart Citizen Kit 2.0 
[![Travis](https://travis-ci.org/fablabbcn/smartcitizen-kit-20.svg?branch=master)](https://travis-ci.org/fablabbcn/smartcitizen-kit-20)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)]()
[![DOI](https://zenodo.org/badge/109865611.svg)](https://zenodo.org/badge/latestdoi/109865611)

![](https://live.staticflickr.com/4781/39073627230_8aee10a859_k.jpg)

## Maybe you are looking for the [SCK 2.1 repository](https://github.com/fablabbcn/smartcitizen-kit-21)

The **SCK 2.0** was the development version for the now commercially available [SCK 2.1](https://github.com/fablabbcn/smartcitizen-kit-21)

## Documentation

* If you want to read more about the Smartcitizen Kit or its sensors please check our [documentation](http://docs.smartcitizen.me/)
* Here you can check the [sensor list](http://docs.smartcitizen.me/Smart%20Citizen%20Kit/#sck-20).
* The SCK provides a comprehensive command shell over USB to manage all the kits functionalities for advanced users: [Using the Smartcitizen Kit shell](http://docs.smartcitizen.me/Components/Firmware/guides/Using%20the%20Shell/) guide.
* A compilation of Smartcitizen hardware work in a single **open publication:** [_Hardware X: Special Issue on Open-Hardware for Environmental Sensing and Instruments_](https://doi.org/10.1016/j.ohx.2019.e00070)
* You can also follow the [forum](https://forum.smartcitizen.me/) and [twitter](https://twitter.com/SmartCitizenKit) for updates.

## Development

In this repository you can find:

* The Smartcitizen Kit 2.1 hardware [design files](./hardware).
* The [bootloader](bootloader) and [main firmware](./sam) (C++) for the SAMD21 microcontroller.
* The [main firmware](./esp) for the ESP8266 microcontroller.
* A [mobile web UI](./mock-api) for setting up the kit.

All branches and pull requests on Github are auto tested with Travis

## Development

The repo consists of 3 things
* The core firmware (C++)
* Frontend, a mobile web UI for setting up the kit. `localhost:8000`
* mock-api - for mocking the API of the kit `localhost:3000`

All branches and pull requests on Github are auto tested with Travis

### The core firmware (C++)

If you just want to upgrade your Smartcitizen kit please refer to the [Updating the Smartcitizen Kit 2.0](./upgrading.md) guide.
But if you want to change something and build the firmware, use the [Developer guide: Building and flashing the Smartcitizen Kit firmware](./building.md) guide.

#### SCK Shell

The SCK provides a comprehensive command shell over USB to manage all the kits functionalities for advanced users. 

_Use any Serial console as `screen`, `platformio device monitor`, or the serial monitor on the Arduino IDE_

Example commands:

```

SCK> help

SCK> config -wifi "myWifiName" "myPassword" -token myToken -mode network

```


### Frontend + api setup
You can see the (master branch) mobile UI setup [here](https://fablabbcn.github.io/smartcitizen-kit-20/esp/build_data/)

The technology used here is:
* HTML, CSS, JavaScript
* Vue.js
* ** Make sure it is using the old javascript, so older phones work. **

And the files are under *esp/build_data*

#### Starting frontend development

Inside the `./mock-api` folder do:

1. `npm install`

1. `npm run web` - Starts frontend on [localhost:8000](http://localhost:8000)

1. `npm run api` - Starts a "mock-api" which emulates the Firmware API on [localhost:3000](http://localhost:3000)

1. `gulp` - Watches changes and creates some files automatically, like: `index.html` and `index.gz`

Now you can start editing **esp/build_data/build_index.html**

If your mock-api is not responding, see */esp/build_data/main.js*, **theUrl** should be (your API url:port)

#### Testing frontend

You can run End to End test (for the Web UI) against the current master branch with this command:

`npm test`

If you want the tests to be run automatically everytime you edit `mock-api/casperjs/test` use:

`npm run autotest`

Edit tests under `mock-api/casperjs/test`

####  TODO / ideas:

- [ ] Should we move the frontend to /mock-api, and create a process which compiles it + concatinates and puts the 'dist' in esp/data?
- [ ] Instead of using a node.js mock-api, can we use the embedded C++ API of the kit somehow?

## Documentation

Full documentation under development. Follow the [forum](https://forum.smartcitizen.me/) and [twitter](https://twitter.com/SmartCitizenKit) for updates.

### The core firmware (C++)

If you just want to upgrade your Smartcitizen kit please refer to the [Updating the Smartcitizen Kit 2.0](./upgrading.md) guide.
But if you want to change something and build the firmware, use the [Developer guide: Building and flashing the Smartcitizen Kit firmware](./building.md) guide.

#### SCK Shell

The SCK provides a comprehensive command shell over USB to manage all the kits functionalities for advanced users. 

_Use any Serial console as `screen`, `platformio device monitor`, or the serial monitor on the Arduino IDE_

Example commands:

```

SCK> help

SCK> config -wifi "myWifiName" "myPassword" -token myToken -mode network

```


### Frontend + api setup
You can see the (master branch) mobile UI setup [here](https://fablabbcn.github.io/smartcitizen-kit-20/esp/build_data/)

The technology used here is:
* HTML, CSS, JavaScript
* Vue.js
* ** Make sure it is using the old javascript, so older phones work. **

And the files are under *esp/build_data*

#### Starting frontend development

Inside the ./mock-api folder do:

1. `npm install`

1. `npm run web` - Starts frontend on [localhost:8000](http://localhost:8000)

1. `npm run api` - Starts api on [localhost:3000](http://localhost:3000)

1. `gulp watch` - Watches changes and creates 2 files automatically; `final.html` and `index.gz`

Now you can start editing **esp/build_data/build_index.html**

If your mock-api is not responding, see */esp/build_data/main.js*, **theUrl** should be (your API url:port)

#### Testing frontend

You can run End to End test (for the Web UI) against the current master branch with this command:

`npm test`

If you want the tests to be run automatically everytime you edit `mock-api/casperjs/test` use:

`npm run autotest`

Edit tests under `mock-api/casperjs/test`

####  TODO / ideas:

- [ ] Should we move the frontend to /mock-api, and create a process which compiles it + concatinates and puts the 'dist' in esp/data?
- [ ] Instead of using a node.js mock-api, can we use the embedded C++ API of the kit somehow?

## Documentation

Full documentation under development. Follow the [forum](https://forum.smartcitizen.me/) and [twitter](https://twitter.com/SmartCitizenKit) for updates.

## Related Smart Citizen repositories

* Platform Core API [github.com/fablabbcn/smartcitizen-api](https://github.com/fablabbcn/smartcitizen-api)
* Platform Web [github.com/fablabbcn/smartcitizen-web](https://github.com/fablabbcn/smartcitizen-web)
* Platform Onboarding [github.com/fablabbcn/smartcitizen-onboarding-app](https://github.com/fablabbcn/smartcitizen-onboarding-app)
* Kit Enclosures [github.com/fablabbcn/smartcitizen-enclosures](https://github.com/fablabbcn/smartcitizen-enclosures)
* Useful software resources for communities [github.com/fablabbcn/smartcitizen-toolkit](https://github.com/fablabbcn/smartcitizen-toolkit)

## License

All the software unless stated is released under [GNU GPL v3.0](https://github.com/fablabbcn/smartcitizen-kit-20/blob/master/LICENSE) and the hardware design files under [CERN OHL v1.2](https://github.com/fablabbcn/smartcitizen-kit-20/blob/master/hardware/LICENSE)

## Funding

This work has received funding from the European Union's Horizon 2020 research and innovation program under the grant agreement [No. 689954](https://cordis.europa.eu/project/rcn/202639_en.html)
