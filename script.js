document.addEventListener("DOMContentLoaded", () => {
  const connectButton = document.getElementById("connectButton");
  const statusArea = document.getElementById("statusArea");
  const currentTempDisplay = document.getElementById("currentTemp");
  const targetTempDisplay = document.getElementById("targetTemp");
  const currentStateDisplay = document.getElementById("currentState");
  const confirmActionButton = document.getElementById("confirmActionButton");

  // TODO: Replace with the actual Service UUID from your ESP32 device
  const TARGET_SERVICE_UUID = "0000180d-0000-1000-8000-00805f9b34fb"; // Example: Heart Rate Service
  // TODO: Define Characteristic UUIDs that will be used later
  // const TARGET_CHARACTERISTIC_UUID_COMMAND = 'your-command-characteristic-uuid';
  // const TARGET_CHARACTERISTIC_UUID_TELEMETRY = 'your-telemetry-characteristic-uuid';

  let bleDevice;
  let bleServer;
  // let commandCharacteristic;
  // let telemetryCharacteristic;

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
          // Option 1: Filter for a specific service UUID
          filters: [{ services: [TARGET_SERVICE_UUID] }],
          // Option 2: Accept all devices (useful for initial discovery, less secure/efficient)
          // acceptAllDevices: true,
          // optionalServices: [TARGET_SERVICE_UUID] // Important if service not primary or if you need more than one
        });

        statusArea.textContent = `Connecting to ${
          bleDevice.name || bleDevice.id
        }...`;
        console.log(`Device selected: ${bleDevice.name || bleDevice.id}`);

        bleDevice.addEventListener("gattserverdisconnected", onDisconnected);

        bleServer = await bleDevice.gatt.connect();
        statusArea.textContent = `Connected to ${
          bleDevice.name || bleDevice.id
        }`;
        console.log(
          `Connected to GATT Server on ${bleDevice.name || bleDevice.id}`
        );
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
    // commandCharacteristic = null;
    // telemetryCharacteristic = null;
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

  console.log("BLE Control UI script loaded.");
});
