# Smart Citizen Kit 2.1 
[![Travis](https://travis-ci.org/fablabbcn/smartcitizen-kit-20.svg?branch=master)](https://travis-ci.org/fablabbcn/smartcitizen-kit-20)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)][![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Ffablabbcn%2Fsmartcitizen-kit-20.svg?type=shield)](https://app.fossa.io/projects/git%2Bgithub.com%2Ffablabbcn%2Fsmartcitizen-kit-20?ref=badge_shield)
()
[![DOI](https://zenodo.org/badge/109865611.svg)](https://zenodo.org/badge/latestdoi/109865611)

[Click here for version 2.0](https://github.com/fablabbcn/smartcitizen-kit-20/tree/2.0)

## Sensor list

|Metric|Units|Sensor|
|-|:-:|:-:|
| **Atmospheric Pressure** | kPa | NXP MPL3115A2 |
| **Air Temperature / Relative Humidity** | ºC / %rh | Sensirion SHT31|
| **VOCS, eCO2** | ppm |AMS CCS811|
| **Noise Level and Spectrum** | dBa |TDK ICS43432|
| **Particle Matter (PM1/2.5/10)** | ug/m3 | Plantower PMS5003 |
| **Ambient Light** | lx | Rohm BH12721FVC |

See the documentation at: http://docs.smartcitizen.me/

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


[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Ffablabbcn%2Fsmartcitizen-kit-20.svg?type=large)](https://app.fossa.io/projects/git%2Bgithub.com%2Ffablabbcn%2Fsmartcitizen-kit-20?ref=badge_large)

## Funding

This work has received funding from the European Union's Horizon 2020 research and innovation program under the grant agreement [No. 689954](https://cordis.europa.eu/project/rcn/202639_en.html)