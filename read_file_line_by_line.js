/**
 * File: read_file_line_by_line.js
 * Description: Demonstrates how to read a file line by line in Espruino
 *
 * Note: This assumes you have a file called "data.txt" on your Espruino's storage.
 * You may need to adjust the filename and path according to your setup.
 */

// Function to read a file line by line
function readFileLineByLine(filename) {
  // Check if the file exists
  try {
    var stat = require("fs").statSync(filename);
    if (!stat) {
      console.log("File doesn't exist: " + filename);
      return;
    }
  } catch (e) {
    console.log("Error accessing file: " + e.toString());
    return;
  }

  // Open the file for reading
  var f = require("fs").openSync(filename, "r");
  if (!f) {
    console.log("Unable to open file: " + filename);
    return;
  }

  var line = "";
  var lineCount = 0;

  // Read characters one by one
  while (true) {
    var buffer = new Uint8Array(1);
    var bytesRead = f.read(buffer, 0, 1);

    // If we've reached the end of the file, break
    if (bytesRead <= 0) break;

    var char = String.fromCharCode(buffer[0]);

    // If we've reached a newline, process the line
    if (char === "\n") {
      console.log("Line " + ++lineCount + ": " + line);
      line = "";
    } else {
      // Add character to the current line
      line += char;
    }
  }

  // Process the last line if it doesn't end with a newline
  if (line.length > 0) {
    console.log("Line " + ++lineCount + ": " + line);
  }

  // Close the file
  f.close();

  console.log("Total lines: " + lineCount);
}

// Alternative approach using a larger buffer for better performance
function readFileLineByLineBuffered(filename, bufferSize) {
  bufferSize = bufferSize || 64; // Default buffer size of 64 bytes

  try {
    var f = require("fs").openSync(filename, "r");
    if (!f) {
      console.log("Unable to open file: " + filename);
      return;
    }

    var buffer = new Uint8Array(bufferSize);
    var line = "";
    var lineCount = 0;

    while (true) {
      var bytesRead = f.read(buffer, 0, bufferSize);
      if (bytesRead <= 0) break;

      for (var i = 0; i < bytesRead; i++) {
        var char = String.fromCharCode(buffer[i]);
        if (char === "\n") {
          console.log("Line " + ++lineCount + ": " + line);
          line = "";
        } else {
          line += char;
        }
      }
    }

    // Process the last line if needed
    if (line.length > 0) {
      console.log("Line " + ++lineCount + ": " + line);
    }

    f.close();
    console.log("Total lines: " + lineCount);
  } catch (e) {
    console.log("Error: " + e.toString());
  }
}

// Example usage
function example() {
  // Create a test file
  var testData =
    "This is line 1\nThis is line 2\nThis is line 3\nThis is the last line";
  require("fs").writeFileSync("data.txt", testData);

  console.log("Reading file line by line:");
  readFileLineByLine("data.txt");

  console.log("\nReading file line by line with buffering:");
  readFileLineByLineBuffered("data.txt");
}

// Run the example - uncomment to execute
// example();

/**
 * To use this code:
 * 1. Save it to your Espruino device
 * 2. Either run the example() function or call readFileLineByLine("your-file.txt")
 * 3. Adjust buffer sizes and file paths as needed for your specific use case
 */
