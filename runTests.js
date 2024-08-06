const { execFile } = require("child_process");
const fs = require("fs").promises;
const path = require("path");
const { exit } = require("process");
const { promisify } = require("util");

const execFilePromise = promisify(execFile);

const ENABLE_DEBUG = process.env["DEBUG"];

if (process.argv.length < 3) {
  console.log("Usage: node runTests.js <path-to-binary-relative-to-build-dir>");
  console.log("Example: node runTests.mjs clox.exe");
  exit(64);
}
const executableRelPath = process.argv[2];

main();

async function main() {
  const tests = await fs.readdir("tests");

  console.log(`running ${tests.length} tests\n`);

  const failedTests = [];
  await Promise.allSettled(tests.map((test) => runTest(test, failedTests)));

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
    console.log("\n");
    await fs.writeFile(
      path.join(__dirname, "tests", failedTest.name, "output.txt"),
      failedTest.output
    );
  }
}

async function runTest(test, failedTests) {
  const cmakeArgs = await getCMakeArgs(test);
  const loxArgs = await getLoxArgs(test);
  const expected = await getExpected(test);
  const tmpDir = await createTmpDir();
  await configure(test, cmakeArgs, tmpDir);
  await build(test, tmpDir);
  const output = (await run(test, loxArgs, tmpDir)).stdout;
  const result = output == expected ? "OK" : "FAILED";
  console.log(`test '${test}' ... ${result}`);

  if (output != expected) {
    failedTests.push({
      name: test,
      output,
      expected,
    });
  }

  await fs.rm(tmpDir, { recursive: true });
}

async function getCMakeArgs(testName) {
  const content = await fs.readFile(
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

async function getLoxArgs(testName) {
  const content = await fs.readFile(
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

async function getExpected(testName) {
  return await fs.readFile(
    path.join("tests", testName, "expected.txt"),
    "utf-8"
  );
}

async function createTmpDir() {
  return await fs.mkdtemp("tmp");
}

async function configure(testName, cmakeArgs, tmpDir) {
  debug(`configuring test '${testName}' with arguments '${cmakeArgs}'`);
  await execFilePromise("cmake", ["-S", __dirname, "-B", tmpDir, ...cmakeArgs]);
}

async function build(testName, tmpDir) {
  debug(`building test '${testName}'`);
  await execFilePromise("cmake", ["--build", tmpDir], { stdio: "ignore" });
}

async function run(testName, args, tmpDir) {
  debug(`running test '${testName}' with arguments '${args}'`);
  const loxFilePath = path.resolve(
    path.join(".", "tests", testName, "test.lox")
  );
  const executablePath = path.resolve(path.join(tmpDir, executableRelPath));
  return await execFilePromise(executablePath, [loxFilePath, ...args]);
}

function debug(...args) {
  if (ENABLE_DEBUG) {
    console.log(...args);
  }
}
