import resolve from "@rollup/plugin-node-resolve";
import commonjs from "@rollup/plugin-commonjs";
import typescript from "@rollup/plugin-typescript";
import babel from "@rollup/plugin-babel";
import { terser } from "rollup-plugin-terser";
import pkg from "./package.json";

// Extensions to process with babel
const extensions = [".js", ".jsx", ".ts", ".tsx"];

// Configuration for both ESM and CommonJS builds
const config = [
  // ESM build for modern environments and bundlers
  {
    input: "index.ts",
    output: {
      file: pkg.module,
      format: "esm",
      sourcemap: true,
      exports: "named",
    },
    plugins: [
      typescript({
        tsconfig: "./tsconfig.json",
        declaration: true,
        declarationDir: "dist",
        sourceMap: true,
      }),
      resolve({ extensions }),
      commonjs(),
    ],
    external: [],
  },
  // CommonJS build for Node.js and other CommonJS environments
  {
    input: "index.ts",
    output: {
      file: pkg.main,
      format: "cjs",
      sourcemap: true,
      exports: "named",
    },
    plugins: [
      typescript({
        tsconfig: "./tsconfig.json",
        declaration: false,
      }),
      resolve({ extensions }),
      commonjs(),
    ],
    external: [],
  },
  // Minified UMD build for direct browser use, with Babel for compatibility
  {
    input: "index.ts",
    output: {
      file: "dist/index.umd.min.js",
      format: "umd",
      name: "FiniteStateMachine",
      sourcemap: true,
      globals: {},
    },
    plugins: [
      typescript({
        tsconfig: "./tsconfig.json",
        declaration: false,
      }),
      resolve({ extensions }),
      commonjs(),
      babel({
        extensions,
        babelHelpers: "bundled",
        include: ["**/*.ts"],
        exclude: "node_modules/**",
      }),
      terser(),
    ],
    external: [],
  },
];

export default config;
