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
  const COMMAND_CONFIRM_ACTION = 0x05; // Example if it's a new command

  let bleDevice;
  let bleServer;
  let commandCharacteristic; // Added for NUS TX characteristic

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
          "Command Characteristic Obtained. Ready to send commands.";
        console.log("Command Characteristic Obtained");

        connectButton.textContent = "Disconnect";
        connectButton.onclick = disconnectDevice; // Change button action to disconnect

        // Next steps (covered in subsequent subtasks):
        // - Get Primary Service
        // - Get Characteristics (command, telemetry)
        // - Start notifications on telemetry characteristic
        // - Enable UI elements for command sending / display
      } catch (error) {
        statusArea.textContent = `Connection Failed: ${error.message}`;
        console.error("Connection failed:", error);
        bleDevice = null; // Reset device on error
        bleServer = null;
        commandCharacteristic = null;
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
    // TODO: Disable UI elements that require a connection
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

  confirmActionButton.addEventListener("click", () => {
    // ... existing code ...
  });

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

  console.log("BLE Control UI script loaded.");
});
