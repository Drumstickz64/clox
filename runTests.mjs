import { execFileSync } from "child_process";
import fs from "fs";
import path from "path";
import { exit } from "process";

if (process.argv.length < 3) {
  console.log("Usage: node runTests.mjs <path-to-binary>");
  exit(64);
}

const executable_path = path.resolve(process.argv[2]);
const tests = fs.readdirSync("tests");

console.log(`running ${tests.length} tests\n`);

const failedTests = [];
for (const test of tests) {
  const args = getArgs(test);
  const expected = getExpected(test);

  const output = run(test, args);
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
}

function getArgs(testName) {
  const content = fs.readFileSync(
    path.join("tests", testName, "args.txt"),
    "utf-8"
  );

  return content.split(" ");
}

function getExpected(testName) {
  return fs.readFileSync(path.join("tests", testName, "expected.txt"), "utf-8");
}

function run(testName, args) {
  const loxFilePath = path.resolve(
    path.join(".", "tests", testName, "test.lox")
  );
  return execFileSync(executable_path, [loxFilePath, ...args], {
    encoding: "utf-8",
  });
}
