# RPY Orientation Comparison

A small local web application for comparing roll, pitch, and yaw (RPY) values from an External IMU and a VectorNav IMU. A C++ HTTP server calculates rotation matrices and component-wise angle differences; an HTML/JavaScript frontend displays the values and visualizes the orientations using Three.js.

## Project layout

```text
project-folder/
├── main.cpp          # C++ HTTP server and rotation calculations
├── httplib.h         # cpp-httplib header (must be available to the compiler)
└── www/
    └── index.html    # Web UI, API calls, and Three.js scene
```

The server serves static files from `./www`, relative to the directory from which the server process is started. Run the executable from the project folder so that `www/index.html` can be found.

## Requirements

- A C++ compiler with C++17 support (or a compatible compiler for this code).
- `httplib.h` from cpp-httplib, available on the include path or next to `main.cpp`.
- A modern browser with WebGL support.
- Internet access for the current frontend to load Three.js 0.128.0 and OrbitControls from jsDelivr. To work offline, download and serve those libraries locally and update the two script paths in `www/index.html`.

## Build and run

Example using GCC or Clang on Linux/macOS, assuming `httplib.h` is beside `main.cpp`:

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o rpy_server
./rpy_server
```

The server listens on `127.0.0.1:8080`. Open this address in your browser:

```text
http://127.0.0.1:8080/
```

Keep the terminal running while using the page. If your existing project uses a different build command, continue using that command.

## How the application works

1. The page starts a Three.js scene and checks the server using `GET /api/hello`.
2. When the page loads, or when **Calculate** is clicked, JavaScript reads six input values: external roll/pitch/yaw and VN roll/pitch/yaw.
3. JavaScript sends them as query parameters to `GET /api/offsets`.
4. C++ validates the parameters, computes the component-wise RPY differences and three matrices, then returns JSON.
5. JavaScript displays the offsets and matrices and applies a display-coordinate mapping to the two sensor frames in Three.js.

### API: health check

`GET /api/hello`

Example response:

```json
{"message":"Hello from the C++ RPY server!"}
```

### API: calculate orientations

`GET /api/offsets` requires all six parameters:

```text
external_roll, external_pitch, external_yaw,
vn_roll, vn_pitch, vn_yaw
```

Example URL (line breaks added for readability; use one line in a browser):

```text
http://127.0.0.1:8080/api/offsets?external_roll=0&external_pitch=0&external_yaw=0&vn_roll=0&vn_pitch=0&vn_yaw=90
```

The JSON response includes:

- `external_rpy`: the external input angles in degrees.
- `vn_rpy`: the VectorNav input angles in degrees.
- `rpy_offsets`: component-wise `VN - External` angle differences in degrees.
- `R_external`: external 3×3 rotation matrix.
- `R_vn`: VectorNav 3×3 rotation matrix.
- `R_offset`: relative rotation matrix, calculated as `transpose(R_external) * R_vn`.

Missing, invalid, non-finite or malformed numeric parameters result in HTTP 400 with an error response.

## Rotation conventions and interpretation

The C++ function `rpyToMatrix()` uses the convention:

```text
R = Rz(yaw) * Ry(pitch) * Rx(roll)
```

Angles are entered in degrees and converted to radians before the trigonometric calculations. The code treats each rotation matrix as mapping coordinates from the sensor's local frame into a common world frame.

The component-wise offset is:

```text
roll_offset  = vn_roll  - external_roll
pitch_offset = vn_pitch - external_pitch
yaw_offset   = vn_yaw   - external_yaw
```

These direct Euler-angle differences are **not generally equivalent** to extracting Euler angles from the relative matrix. The relative matrix is calculated as:

```text
R_offset = transpose(R_external) * R_vn
```

This formula assumes both input matrices map from their respective local sensor frames into the same world frame.

### NED and ENU display selector

The selector changes the visualization's world-coordinate mapping; it does **not** reinterpret the input angles or recalculate the backend matrices under a different physical convention.

- **NED:** North, East, Down. The display mapping sends `(x, y, z)` to `(x, -z, y)`.
- **ENU:** East, North, Up. The display mapping sends `(x, y, z)` to `(x, z, -y)`.

The frontend's `toDisplayMatrix()` uses `R_display = C * R_math`. This maps the world-coordinate basis for rendering while leaving the sensor frame's local geometry in Three.js local X/Y/Z. Avoid changing this to `C * R_math * Cᵀ` without revisiting the local-frame geometry and validating the result: that change can make the zero-angle frame fail to align with the displayed world axes.

The correct physical interpretation still depends on the actual axis definitions and Euler-angle convention used by each IMU. The current code implements the convention documented above; it does not automatically convert between two different sensor conventions.

## Frontend code map

In `www/index.html`:

- **HTML near the top of `<body>`:** input fields, buttons, offsets, matrix areas, display selector, and checkboxes.
- **CSS in `<style>`:** layout, colors, responsive behavior, and panel/scene sizing.
- `readInputs()`: reads and validates the six input fields.
- `calculate()`: calls `/api/offsets`, checks the response, and updates the UI.
- `renderOffsets()` and `renderMatrix()`: display numeric output.
- `toMatrix3()` and `matrix3ToMatrix4()`: convert matrix data into the forms Three.js expects.
- `toDisplayMatrix()`: maps a backend matrix to the selected visualization basis.
- `createAxis()`, `makeFrame()`, and `buildScene()`: create the sensor axes, camera, renderer, lighting, grid, and mouse controls.
- `updateWorldAxes()` and `updateSceneMatrices()`: update the world convention and sensor orientations.
- `resetInputs()`: zeroes all angle inputs and recalculates.

In `main.cpp`:

- `rpyToMatrix()`: converts RPY angles into a rotation matrix.
- `multiply()` and `transpose()`: basic matrix operations.
- `calculateRPYOffsets()`: calculates direct VN-minus-External angle differences.
- `readParameter()` and `readRPYParameters()`: validate URL parameters.
- `/api/hello`: simple server health check.
- `/api/offsets`: performs calculations and returns JSON.
- `server.set_mount_point("/", "./www")`: serves the frontend files.

## Suggested sanity checks

1. **All six angles zero:** all three returned matrices should be identity matrices, all component-wise offsets should be zero, and the two sensor frames should align with each other and the world axes.
2. **External zero; VN yaw = 90°:** the component-wise yaw offset should be 90°. Inspect the VN frame's orientation against the documented `Rz * Ry * Rx` convention.
3. **External zero; VN pitch = 90°:** the component-wise pitch offset should be 90°. Check the resulting matrix and 3D orientation.
4. **External and VN identical:** all component-wise offsets should be zero and `R_offset` should be the identity matrix (allowing for floating-point rounding).
5. **Switch NED/ENU:** the display basis and world-axis labels should change, while the numerical matrices and offsets returned by the server should remain the same.

Change one angle at a time during tests. The apparent screen direction depends on the camera viewpoint, so use the matrix values and stated coordinate convention as well as the picture when checking correctness.

## Troubleshooting

- **Page does not load:** make sure the server is running and that you started it from the directory containing `www/`.
- **Server connection error:** open `http://127.0.0.1:8080/api/hello` directly. Check that the process is running and port 8080 is available.
- **Three.js or OrbitControls error:** check internet access and the browser Developer Tools Console/Network tab for failed CDN requests.
- **Inputs do not update the display:** click **Calculate**, then inspect the browser Console and the Network request to `/api/offsets`.
- **HTTP 400 from `/api/offsets`:** ensure all six parameters are present and are finite numbers.
- **Axes appear wrong:** verify the backend's rotation convention, `toDisplayMatrix()`, and the NED/ENU selector separately. Avoid changing backend math just to compensate for a rendering-basis issue.

## Development notes

- Use browser Developer Tools (usually `F12`) to inspect JavaScript errors and network requests.
- Keep a backup or use Git before making changes.
- The server currently binds to `127.0.0.1`, so it is intended for local access on the same machine. Do not expose it to a network without reviewing security and deployment requirements.
- `main.cpp` contains a TODO suggesting that the example `/api/hello` endpoint be removed before deployment. It is useful during local development as a health check.
