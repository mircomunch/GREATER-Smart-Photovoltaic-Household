## Description

[Items](https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/wiki/Items) | [Tutorial](https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/wiki/Tutorial) | [References](https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/wiki/References)

This kit replicates a **miniaturized smart photovoltaic household
system**, integrating power generation, energy management, monitoring,
and control functions into a compact and portable format. It provides a
realistic hands-on environment that allows users to work with the
essential components of a modern smart energy system.

The system gives learners practical exposure to photovoltaic energy
generation, battery storage, DC and AC load management, and power
conversion. In addition, it collects electrical and environmental
parameters that can be monitored, stored, or used to implement advanced
control strategies and smart services.

The overall architecture consists of two main layers:

1.  **Hardware Layer** – power devices, sensors, and a microcontroller
    responsible for data acquisition and system interaction.

2.  **Cloud/Software Layer** – a locally hosted cloud environment (e.g.,
    Node-RED) used to visualize data, store measurements, and run
    control logic.

Communication between these layers is handled through an **MQTT
broker**, enabling flexible and efficient data exchange.

The complete hardware setup is mounted inside a **portable suitcase**,
making the kit easy to transport and deploy. Although specific devices
are shown in the reference design, the system is modular: key components
such as the PV panel, battery, and inverter may be substituted as long
as their technical specifications remain compatible with the
architecture.

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image2.png" width="605" />
<a name="fig1"></a>

_Fig.1 Architecture_

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image3.jpeg" width="401" />
<a name="fig2"></a>

_Fig.2 Closed suitcase_

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image4.jpeg" width="514" />
<a name="fig3"></a>

_Fig.3 Opened suitcase_

### Working mode

The following overview clarifies how the system operates.

The **charge controller** manages the energy flow between the PV panel,
the battery, and a 12 V DC load (a LED bulb). It acts as the central
power-management device, maintaining proper charging conditions and
distributing power as needed.

The **microcontroller** **ESP32** communicates with the charge
controller via **RS485 using the Modbus protocol**, allowing voltage,
current, and other operational parameters to be monitored.

Power stored in the battery is also fed to an **inverter**, which
converts DC into AC to supply an AC light bulb. The AC power path is
monitored by an **AC power meter** and controlled using a relay driven
by the microcontroller. Because the relay requires a different supply
voltage, a small auxiliary power supply (e.g., an old laptop adapter) is
used together with a simple pull-down control circuit.

All data acquired by the microcontroller are transmitted via **MQTT**
over a Wi-Fi connection. Once received by the MQTT broker hosted on the
*HiveMQ* cloud service, these data are accessed by a **Node-RED–based
software application** for visualization, processing, and control. In
the same way, commands issued through the Node-RED user interface are
sent back to the microcontroller via MQTT, enabling remote control of
the loads and management of the system’s operation.

Although the Node-RED application runs locally on a PC, its full
integration with cloud services allows it to function as the cloud-based
component of the architecture.

This architecture allows students to observe real-time data from both DC
and AC sides of the system while issuing control commands through the
cloud platform.

[![See the system overview](https://img.youtube.com/vi/tfOb2_4k6YQ/0.jpg)](https://www.youtube.com/watch?v=tfOb2_4k6YQ)


<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image5.png" width="298" />
<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image6.png" width="605" />
<a name="fig4"></a>

_Fig.4 Devices with labels, top part_

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image7.png" width="605" />
<a name="fig5"></a>

_Fig.5 Devices with labels, bottom part_

To start the system, the battery must first be connected to both the
charge controller and the inverter (in the example setup, this is done
by turning on the dedicated switch). This connection also powers the
microcontroller through the inverter’s USB port. Once powered, the
microcontroller searches for a known Wi-Fi network and attempts to
connect to the configured MQTT broker using the parameters defined in
its setup file. After establishing both connections, the device waits to
synchronize its internal clock, receiving the current date and time in
Unix format (milliseconds elapsed since 1 gen 1970) through a dedicated
MQTT topic.

The microcontroller’s main process can be enabled through the “Run”
command, which is issued from the Node-RED user interface and
transmitted via another MQTT topic. Once the hardware receives both the
time synchronization and the run command, it begins its operating cycle:
it reads all available data from the connected devices and publishes
them via MQTT while continuously listening for new commands to execute.

All command handling is managed in Node-RED through the dashboard
interface.

At the user interface level, the dashboard provides three pages that
demonstrate the core functionalities and serve as an example for future
feature development:

- Home

- Charts

- Log

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image8.png" width="605" />
<a name="fig6"></a>

_Fig.6 Home page of Dashboard_

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image9.png" width="605" />
<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image10.png" width="605" />
<a name="fig7"></a>

_Fig.7 Charts page of Dashboard_

<img src="https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household/blob/main/wiki/images/image11.png" width="605" />
<a name="fig8"></a>

_Fig.8 Log page of Dashboard_

The complete Node-RED application is organized into seven flows:

- **Dummy uC** – Simulates a microcontroller by generating and sending
  random data.

- **Commands management** – Handles commands such as updating the device
  timestamp, switching loads, deleting stored data, and clearing
  database tables.

- **Receive and store data** – Performs initialization at startup,
  listens for incoming data, parses and stores them, and manages
  available device IDs and their last-seen timestamps.

- **Info Page** – Resets homepage data and manages the “Battery State of
  Charge” gauge.

- **Generic Data Chart** – Manages the final chart of the “Charts” page
  by selecting available data for the chosen device ID and plotting
  stored values.

- **Charts Page** – Clears chart elements and renders all available data
  grouped by data type.

- **Log Page** – Displays the database tables in a graphical format.

### Kit opportunity

Once assembled, the kit offers extensive learning opportunities across
hardware, firmware, and software domains. It allows students to explore
realistic energy-system behaviour, integrate multiple technologies, and
experiment with control strategies using real data.

#### Examples of Learning Activities

#### Software

- Designing multiple user interfaces using the same data set.

- Experimenting with MQTT communication techniques.

- Implementing alarms, notifications, and safety conditions.

- Using databases for data logging and analysis.

- Adding multi-user management and authentication features.

#### Hardware

- Modifying the power rating of the PV, load, or storage system.

- Adding additional sensors (current, voltage, temperature, etc.).

- Integrating new actuators such as relays, switches, or smart loads.

#### Firmware

- Implementing new command structures or control routines.

- Reading and integrating additional sensors.

- Extending wireless communication (Wi-Fi, BLE, etc.).

- Modifying embedded-system logic, timers, and interrupt routines.

The modularity of the system allows learners to work at one level:
hardware, firmware, or software; or to combine them for more advanced
activities.