document.addEventListener("DOMContentLoaded", () => {
  const connectButton = document.getElementById("connectButton");
  const statusArea = document.getElementById("statusArea");
  const currentTempDisplay = document.getElementById("currentTemp");
  const targetTempDisplay = document.getElementById("targetTempDisplay");
  const currentStateDisplay = document.getElementById("currentState");
  const confirmActionButton = document.getElementById("confirmActionButton");

  // New UI elements for Subtask 15.2
  const setTargetTempInput = document.getElementById("setTargetTempInput");
  const setTargetTempButton = document.getElementById("setTargetTempButton");
  const startProcessButton = document.getElementById("startProcessButton");
  const stopProcessButton = document.getElementById("stopProcessButton");
  const getStatusButton = document.getElementById("getStatusButton");

  // UUIDs - Updated for Nordic UART Service (NUS)
  const TARGET_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const COMMAND_CHARACTERISTIC_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // NUS TX
  const TELEMETRY_CHARACTERISTIC_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // NUS RX (for later)

  // Command IDs
  const COMMAND_SET_TARGET_TEMP = 0x01;
  const COMMAND_START_PROCESS = 0x02;
  const COMMAND_STOP_PROCESS = 0x03;
  const COMMAND_GET_STATUS = 0x04;
  const COMMAND_CONFIRM_ACTION = 0x05; // Added for confirmation

  let bleDevice;
  let bleServer;
  let commandCharacteristic; // Added for NUS TX characteristic
  let telemetryCharacteristic; // Added for NUS RX characteristic
  let lastMessageIdRequiringConfirmation = null; // To store message ID

  // Map for device states
  const DEVICE_STATES = {
    0: "Idle",
    1: "Heating",
    2: "Cooling",
    3: "Maintaining Temp",
    4: "Process Active",
    5: "Error",
    // Add more states as defined by your device firmware
  };

  if (connectButton) {
    connectButton.onclick = async () => {
      if (!navigator.bluetooth) {
        statusArea.textContent =
          "Web Bluetooth API is not available in this browser.";
        console.error("Web Bluetooth API not available.");
        return;
      }

      try {
        statusArea.textContent = "Requesting Bluetooth Device...";
        console.log("Requesting Bluetooth device...");

        bleDevice = await navigator.bluetooth.requestDevice({
          filters: [{ services: [TARGET_SERVICE_UUID] }],
          // optionalServices: [TARGET_SERVICE_UUID] // Can also use optionalServices if primary service is not advertised
        });

        statusArea.textContent = `Connecting to ${
          bleDevice.name || bleDevice.id
        }...`;
        console.log(`Device selected: ${bleDevice.name || bleDevice.id}`);

        bleDevice.addEventListener("gattserverdisconnected", onDisconnected);

        bleServer = await bleDevice.gatt.connect();
        statusArea.textContent = "Connected to GATT Server. Getting Service...";
        console.log("Connected to GATT Server");

        const service = await bleServer.getPrimaryService(TARGET_SERVICE_UUID);
        statusArea.textContent =
          "Service Obtained. Getting Command Characteristic...";
        console.log("Service Obtained");

        commandCharacteristic = await service.getCharacteristic(
          COMMAND_CHARACTERISTIC_UUID
        );
        statusArea.textContent =
          "Command Characteristic Obtained. Getting Telemetry Characteristic...";
        console.log("Command Characteristic Obtained");

        telemetryCharacteristic = await service.getCharacteristic(
          TELEMETRY_CHARACTERISTIC_UUID
        );
        statusArea.textContent =
          "Telemetry Characteristic Obtained. Starting notifications...";
        console.log("Telemetry Characteristic Obtained");

        await telemetryCharacteristic.startNotifications();
        telemetryCharacteristic.addEventListener(
          "characteristicvaluechanged",
          handleTelemetry
        );
        statusArea.textContent = "Notifications started. Ready.";
        console.log("Telemetry notifications started.");

        connectButton.textContent = "Disconnect";
        connectButton.onclick = disconnectDevice; // Change button action to disconnect

        // TODO: Disable UI elements that require a connection
        currentTempDisplay.textContent = "N/A";
        targetTempDisplay.textContent = "N/A";
        currentStateDisplay.textContent = "N/A";
        confirmActionButton.style.display = "none";
        lastMessageIdRequiringConfirmation = null;
      } catch (error) {
        statusArea.textContent = `Connection Failed: ${error.message}`;
        console.error("Connection failed:", error);
        bleDevice = null; // Reset device on error
        bleServer = null;
        commandCharacteristic = null;
        telemetryCharacteristic = null;
      }
    };
  }

  function onDisconnected(event) {
    const device = event.target;
    statusArea.textContent = `Device ${device.name || device.id} disconnected.`;
    console.log(`Device ${device.name || device.id} disconnected.`);
    connectButton.textContent = "Connect to Device";
    connectButton.onclick = connectButton.onclick; // This needs to re-assign the original connection logic
    // For simplicity in this stage, we re-assign the initial onclick directly.
    // A more robust approach would be to have separate connect/disconnect functions.
    // Or, re-initialise the original event listener as defined above.
    bleDevice = null;
    bleServer = null;
    commandCharacteristic = null;
    telemetryCharacteristic = null;
    // TODO: Disable UI elements that require a connection
    currentTempDisplay.textContent = "N/A";
    targetTempDisplay.textContent = "N/A";
    currentStateDisplay.textContent = "N/A";
    confirmActionButton.style.display = "none";
    lastMessageIdRequiringConfirmation = null;
  }

  async function disconnectDevice() {
    if (!bleDevice) {
      console.log("No device connected.");
      return;
    }
    if (bleDevice.gatt.connected) {
      statusArea.textContent = `Disconnecting from ${
        bleDevice.name || bleDevice.id
      }...`;
      console.log(`Disconnecting from ${bleDevice.name || bleDevice.id}`);
      try {
        await bleDevice.gatt.disconnect();
        // The 'gattserverdisconnected' event should fire, calling onDisconnected()
      } catch (error) {
        statusArea.textContent = `Error disconnecting: ${error.message}`;
        console.error("Error disconnecting:", error);
        // Even on error, call onDisconnected to attempt cleanup
        onDisconnected({ target: bleDevice });
      }
    } else {
      console.log("Device already disconnected.");
      onDisconnected({ target: bleDevice }); // Ensure UI updates if already disconnected
    }
  }

  // Placeholder for functions to be developed in other subtasks
  // async function sendCommand(command) { ... }
  // function handleTelemetry(event) { ... }
  // function updateGraph(tempData) { ... }
  // function updateConfirmButtonVisibility(state) { ... }

  // Placeholder onclick handlers for new buttons (functionality in later subtasks)
  if (setTargetTempButton) {
    setTargetTempButton.onclick = () => {
      const tempValue = parseInt(setTargetTempInput.value);
      if (isNaN(tempValue) || tempValue < 0 || tempValue > 100) {
        // Basic validation
        statusArea.textContent =
          "Invalid target temperature. Please enter a value between 0 and 100.";
        console.error(
          "Invalid target temperature value:",
          setTargetTempInput.value
        );
        return;
      }
      console.log("Set Target Temperature button clicked. Value:", tempValue);
      sendCommand(COMMAND_SET_TARGET_TEMP, tempValue);
    };
  }

  if (startProcessButton) {
    startProcessButton.onclick = () => {
      console.log("Start Process button clicked.");
      sendCommand(COMMAND_START_PROCESS);
    };
  }

  if (stopProcessButton) {
    stopProcessButton.onclick = () => {
      console.log("Stop Process button clicked.");
      sendCommand(COMMAND_STOP_PROCESS);
    };
  }

  if (getStatusButton) {
    getStatusButton.onclick = () => {
      console.log("Get Device Status button clicked.");
      sendCommand(COMMAND_GET_STATUS);
    };
  }

  if (confirmActionButton) {
    // Ensure button exists
    confirmActionButton.onclick = () => {
      if (lastMessageIdRequiringConfirmation !== null) {
        console.log(
          `Confirm Action button clicked for message ID: ${lastMessageIdRequiringConfirmation}`
        );
        // Send a confirmation command, possibly including the message ID
        sendCommand(COMMAND_CONFIRM_ACTION, lastMessageIdRequiringConfirmation);
        confirmActionButton.style.display = "none"; // Hide after click
        lastMessageIdRequiringConfirmation = null; // Reset
        statusArea.textContent = "Confirmation sent.";
      } else {
        console.warn(
          "Confirm button clicked but no action was pending confirmation."
        );
        confirmActionButton.style.display = "none"; // Hide anyway
      }
    };
  }

  // --- Command Sending Function ---
  async function sendCommand(commandId, value = null) {
    if (!bleDevice || !bleDevice.gatt.connected || !commandCharacteristic) {
      statusArea.textContent =
        "Device not connected or command characteristic not available.";
      console.error(
        "Error: Device not connected or command characteristic not available."
      );
      return;
    }

    let buffer;
    if (value !== null) {
      buffer = new ArrayBuffer(2);
      const dataView = new DataView(buffer);
      dataView.setUint8(0, commandId);
      dataView.setUint8(1, value);
    } else {
      buffer = new ArrayBuffer(1);
      const dataView = new DataView(buffer);
      dataView.setUint8(0, commandId);
    }

    try {
      console.log(
        `Sending command: ID=${commandId}, Value=${value}, Buffer=${Array.from(
          new Uint8Array(buffer)
        )}`
      );
      statusArea.textContent = `Sending command ${commandId}...`;
      await commandCharacteristic.writeValueWithResponse(buffer);
      // For NUS, often writeWithoutResponse is used for TX, but WithResponse is safer to start with.
      // await commandCharacteristic.writeValueWithoutResponse(buffer);
      statusArea.textContent = `Command ${commandId} sent successfully.`;
      console.log(`Command ${commandId} sent.`);
    } catch (error) {
      statusArea.textContent = `Error sending command ${commandId}: ${error.message}`;
      console.error(`Error sending command ${commandId}:`, error);
    }
  }

  // --- Handle Telemetry (Incoming Data) ---
  function handleTelemetry(event) {
    const value = event.target.value; // This is a DataView
    if (!value || value.byteLength === 0) {
      console.warn("Received empty telemetry data.");
      return;
    }

    const messageType = value.getUint8(0);
    console.log(
      `Received Telemetry: Type=${messageType}, Data=${Array.from(
        new Uint8Array(value.buffer)
      )}`
    );

    try {
      switch (messageType) {
        case 0x10: // Device Status Update
          if (value.byteLength < 4) {
            console.error("Status Update (0x10) too short:", value.byteLength);
            statusArea.textContent = "Error: Malformed status update.";
            return;
          }
          const currentTemp = value.getUint8(1);
          const targetTemp = value.getUint8(2);
          const stateByte = value.getUint8(3);

          currentTempDisplay.textContent = `${currentTemp} °C`;
          targetTempDisplay.textContent = `${targetTemp} °C`;
          currentStateDisplay.textContent =
            DEVICE_STATES[stateByte] || `Unknown (${stateByte})`;
          statusArea.textContent = "Device status updated.";
          break;

        case 0x11: // Confirmation Required
          if (value.byteLength < 2) {
            console.error(
              "Confirmation Required (0x11) too short:",
              value.byteLength
            );
            statusArea.textContent = "Error: Malformed confirmation request.";
            return;
          }
          lastMessageIdRequiringConfirmation = value.getUint8(1);
          confirmActionButton.style.display = "block";
          statusArea.textContent = `Action (ID: ${lastMessageIdRequiringConfirmation}) requires confirmation.`;
          console.log(
            `Confirmation required for message ID: ${lastMessageIdRequiringConfirmation}`
          );
          break;

        case 0x20: // Simple Log Message (Optional)
          const decoder = new TextDecoder("utf-8");
          const logMessage = decoder.decode(value.buffer.slice(1)); // Slice off the message type byte
          statusArea.textContent = `Log: ${logMessage}`;
          console.log(`Device Log: ${logMessage}`);
          break;

        default:
          console.warn("Received unknown telemetry message type:", messageType);
          statusArea.textContent = `Received unknown data type: ${messageType}`;
          break;
      }
    } catch (error) {
      console.error("Error processing telemetry:", error);
      statusArea.textContent = `Error processing data: ${error.message}`;
    }
  }

  console.log("BLE Control UI script loaded.");
});
