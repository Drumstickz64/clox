const { execFileSync } = require("child_process");
const fs = require("fs");
const path = require("path");
const { exit } = require("process");

const ENABLE_DEBUG = process.env["DEBUG"];

if (process.argv.length < 3) {
  console.log(
    "Usage: node runTests.mjs <path-to-build-directory> <path-to-binary>"
  );
  console.log("Example: node runTests.mjs build/gcc/ build/gcc/clox.exe");
  exit(64);
}

const buildDir = process.argv[2];
debug("Build directory =", buildDir);
const executablePath = path.join(__dirname, process.argv[3]);
const tests = fs.readdirSync("tests");

if (!fs.existsSync(buildDir)) {
  console.error(
    `build directory '${buildDir}' not found, make sure you've run CMake configure`
  );
}

console.log(`running ${tests.length} tests\n`);

const failedTests = [];
for (const test of tests) {
  const cmakeArgs = getCMakeArgs(test);
  const loxArgs = getLoxArgs(test);
  const expected = getExpected(test);
  configure(test, cmakeArgs);
  build(test);
  const output = run(test, loxArgs);
  const result = output == expected ? "OK" : "FAILED";
  console.log(`test '${test}' ... ${result}`);

  if (output != expected) {
    failedTests.push({
      name: test,
      output,
      expected,
    });
  }
}

console.log("\nfinished running tests");

if (failedTests.length > 0) {
  console.log("\n-------- failures --------\n");
}

for (const failedTest of failedTests) {
  console.log(`---- ${failedTest.name} ----\n`);
  console.log("output:");
  console.log(failedTest.output);
  console.log("expected:");
  console.log(failedTest.expected);
  fs.writeFileSync(
    path.join(__dirname, "tests", failedTest.name, "output.txt"),
    failedTest.output
  );
}

function getCMakeArgs(testName) {
  const content = fs.readFileSync(
    path.join("tests", testName, "args.txt"),
    "utf-8"
  );

  const cmakeLine = content.split("\n")[0];

  return cmakeLine
    .slice("cmake:".length)
    .trim()
    .split(" ")
    .filter((arg) => arg);
}

function getLoxArgs(testName) {
  const content = fs.readFileSync(
    path.join("tests", testName, "args.txt"),
    "utf-8"
  );

  const loxLine = content.split("\n")[1];

  return loxLine
    .slice("lox:".length)
    .trim()
    .split(" ")
    .filter((arg) => arg);
}

function getExpected(testName) {
  return fs.readFileSync(path.join("tests", testName, "expected.txt"), "utf-8");
}

function configure(testName, cmakeArgs) {
  debug(`configuring test '${testName}' with arguments '${cmakeArgs}'`);
  execFileSync("cmake", ["-S", __dirname, "-B", buildDir, ...cmakeArgs]);
}

function build(testName) {
  debug(`building test '${testName}'`);
  execFileSync("cmake", ["--build", buildDir]);
}

function run(testName, args) {
  debug(`running test '${testName}' with arguments '${args}'`);
  const loxFilePath = path.resolve(
    path.join(".", "tests", testName, "test.lox")
  );
  return execFileSync(executablePath, [loxFilePath, ...args], {
    encoding: "utf-8",
  });
}

function debug(...args) {
  if (ENABLE_DEBUG) {
    console.log(...args);
  }
}
