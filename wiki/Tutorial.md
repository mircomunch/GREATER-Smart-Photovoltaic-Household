# Tutorial

[Description](https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/wiki/Description) | [Items](https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/wiki/Items) | [References](https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/wiki/References)

The construction of the kit involves three main stages:

1.  **Connecting the power components**

2.  **Connecting the data-communication components**

3.  **Uploading the firmware and launching the cloud application**

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image29.png" width="612" />

<a name="fig1"></a>

_Fig.1 Schematic_

# Step 1: Power Connections

1.  Prepare a suitable frame to mount all devices. If building a
    portable kit, secure all components firmly.

2.  Use the back panel to organize the components and maintain a clean
    layout.

3.  Cut wires to appropriate lengths and attach the correct connectors.
    Use appropriately sized cables according to the expected current.

4.  Build the “Control Electronics” block for driving the relay when its
    coil voltage differs from the microcontroller supply (e.g., 24 V).
    This block uses an NPN transistor operated via a microcontroller
    GPIO, with an antiparallel diode added for protection. See [Figure 2](#fig2)
    for further details.

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image30.png" width="471" />
<a name="fig2"></a>

_Fig.2 Pull-down scheme_

1.  When GPIO = 0V → VR = VBE = 0V → BJT OFF → IC = Ion = 0A → Relay OFF
    → 14 connected to 12

2.  When GPIO = 3.3V → BJT ON → VBE = 0.75V → VR = 2.55V → IB = VR/R
    (R=2kΩ) = 1.28mA (\< max Current GPIO = 12mA) → IC = Ion = IB\*β
    (β=150) = 191mA (\> min ON current = 16mA) → Relay ON → 14 connected
    to 11

5.  Proceed with connections: from 1 to 27. See [Figure 1](#fig1)

    1.  The Current Clamp (CT) must be placed around the live wire that
        goes to the AC load

    2.  Terminals, connectors and cable lug will be used according to
        the connection type

6.  Test system functionality:

    1.  Verify that the PV panel charges the battery

    2.  Check DC load operation

    3.  Test inverter operation

    4.  Confirm that the relay switches the AC load correctly

(Optional) Install a battery-disconnect switch on the negative pole.


# Step 2: Data Transmission Connections


1.  Mount the microcontroller on a breadboard or PCB.

2.  Prepare an RJ45 cable to connect the MAX485 module to the charge
    controller’s RS485 COM port (pins 4, 5, and 8). See [Figure 3](#fig3)

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image31.png" width="326" />

<a name="fig3"></a>

_Fig.3 RJ45 Cable connection_

3.  Proceed with all other connections from 28 to 44. See [Figure 1](#fig1)

(Optional) Microcontroller pins may be reassigned, but changes must be
reflected in the firmware.

# Step 3: Firmware and Cloud Application

1.  Choose an MQTT Broker provider. This can be a public broker (e.g.,
    test.mosquitto.org, broker.hivemq.com), a free cloud-based service
    such as HiveMQ, or your own MQTT broker if you can make it
    accessible from external networks.

2.  Set up a Wi-Fi network. This will be used by the microcontroller to
    send and receive data. The data are organized in JSON objects with
    identifiers for each type of measurement ([Figure 4](#fig4)). This allows
    new data types to be added without modifying the entire
    structure—only new identifiers must be introduced. The currently
    available identifiers are:

    - batt_voltage

    - batt_current

    - batt_soc

    - pv_current

    - pv_voltage

    - load_current

    - load_voltage

    - env_temperature

    - env_humidity

    - inv_voltage

    - inv_current

    - inv_power

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image32.png" width="228" />
<a name="fig4"></a>

_Fig.4 JSON data format_

Similarly, commands are also structured as JSON objects, following the
format shown in [Figure 5](#fig5):

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image33.png" width="178" />
<a name="fig5"></a>

_Fig.5 JSON command format_

3.  Clone the GitHub directory containing the firmware. You can either
    download the ZIP archive from GitHub (under Code → Download ZIP) or
    install the GitHub CLI tools and execute *git clone
    \<https_github_link\>.*  
    The firmware consists of three sections, as shown in [Figure 6](#fig6):

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image34.png" width="189" />
<a name="fig6"></a>

_Fig.6 Firmware contents_

- **lib**: Libraries for the modules used. These files contain custom
  methods based on third-party libraries. This section does not need to
  be modified if you only want to run the default setup. Includes:

  - *powerReader*

  - *solarModbus*

  - *MQTT*

  - *DHT_temphum*

- **include**: Setup files for the modules. Each file contains
  parameters needed to customize the firmware. Here you must insert
  configuration values (such as the MQTT broker, Wi-Fi credentials, or
  specific pins used) to make the hardware operate correctly. Below are
  the major parameters. Those in **bold** within *setup.hpp* must be
  configured. The files also specify data identifiers,
  serial-communication parameters, and the number of retries before a
  reading is considered failed. These are mainly for advanced users.

  - *acRele.hpp*

    - RELE_PIN - Number of the pin used for Relay

  - *dht11_setup.hpp*

    - DHTPIN - Number of the pin used for DHT sensor

  - *powerReader_setup.hpp*

    - JSY_PIN_RX - Receiver pin from JSY sensor

    - JSY_PIN_TX - Transmitter pin from JSY sensor

    - JSY_CLAMP - Clamp number to be read

  - *solarModbus_setup.hpp*

    - PIN_RE_DE - Enable-communication pin for the MAX485

    - PIN_RO - Receive-data pin for the MAX485

    - PIN_DI - Send-data pin for the MAX485

  - *setup.hpp*

    - **BOARD_ID** - Device unique identifier

    - **LOCATION** - Device installation site

    - SENS_TEMPHUM - Enables DHT sensor data reading (if not commented)

    - SENS_POWERINV - Enables inverter power measurement reading (if not
      commented)

    - **WIFI_SSID** - Wi-Fi network name

    - **WIFI_PASSWORD** - Wi-Fi password

    - MQTT_AUTH - Enables authenticated MQTT connection (if not
      commented)

    - **MQTT_HOST** - Broker server name

    - **MQTT_PORT** - Broker port

    - **MQTT_USER** - MQTT username (if authentication enabled)

    - **MQTT_PASSWORD** - MQTT password (if authentication enabled)

    - **MQTT_TOPIC** - Root MQTT topic

    - **MQTT_PUBLISH_TOPIC** - Topic used for publishing data (appended
      to root)

    - **MQTT_DATETIME_TOPIC** - Topic for receiving datetime updates
      (appended to root)

    - **MQTT_CMD_TOPIC** - Topic for receiving commands (appended to
      root)

    - DATA_TIMER - Time interval for data transmission

    - LOAD_TIMER - Time interval for handling commands

- **Scr**: Contains the main file with core functionalities. The system
  is based on two timers: one for reading data from all devices and
  another for handling received commands. Reading functions come from
  the libraries, and this file includes methods to format the data into
  JSON before transmission.

4.  Configure the firmware based on your Wi-Fi and MQTT broker settings
    using the parameters described above.

5.  Download and install Node-RED

6.  Before starting, enable the “Projects” feature in the *settings.js*
    file. This allows you to load the entire application and its
    configuration directly from GitHub. To enable projects, open
    *settings.js* at *C:\Users\\your_user_name\>\\node-red* and modify
    the section shown in [Figure 7](#fig7).

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image35.png" width="165" />
<a name="fig7"></a>

_Fig.7 Enable projects setting_

7.  Once Node-RED is installed with the Projects feature, it will prompt
    you at startup to configure the project:

<table>
<colgroup>
<col style="width: 65%" />
<col style="width: 35%" />
</colgroup>

<tbody>
<tr class="odd">
<td><p><img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image36.png" width="283" /></p>
<p><i>Project startup</i></p></td>
<td>Select “Clone Repository”</td>
</tr>
<tr class="even">
<td><p><img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image37.png" width="283" /></p>
<p><i>Project username</i></p></td>
<td>Enter a username and email address. This is only required for Git
operations; <strong>no GitHub account is needed</strong>.</td>
</tr>
<tr class="odd">
<td><p><img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image38.png" width="283" /></p>
<p><i>Project GitHub source code link</i></p></td>
<td><p>Insert in the “Git repository URL” the Git project url:</p>
<p>https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household.git</p></td>
</tr>
<tr class="even">
<td><p><img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image39.png" width="283" /></p>
<p><i>Package update folder</i></p></td>
<td>In Menù → Projects → Projects Settings → Settings update the
package.json path by selecting the one inside the “Node-RED application”
folder</td>
</tr>
<tr class="odd">
<td><p><img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image40.png" width="283" /></p>
<p><i>Project update dependencies</i></p></td>
<td>In Menù → Projects → Projects Settings → Dependencies install the
required plugins</td>
</tr>
</tbody>
</table>

You may need to remove unused “Configuration Nodes” on the right sidebar
(shown only as numeric blocks), as they can prevent the application from
deploying.

8.  Complete the setup by configuring the environment variables in the
    palette manager: Right Top Menu → Manage Palette → Environment

    - DB_PATH

    - MQTT_HOSTNAME

    - MQTT_PORT

    - MQTT_USR

    - MQTT_PSW

    - MQTT_TOPIC_DATA

    - MQTT_TOPIC_DATETIME

    - MQTT_TOPIC_CMD