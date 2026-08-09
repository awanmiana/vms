function createTestHarness(label) {
  let failures = 0;

  async function run(name, test) {
    try {
      await test();
      console.log(`ok - ${name}`);
    } catch (error) {
      failures += 1;
      console.error(`FAIL - ${name}`);
      console.error(error);
    }
  }

  function runSync(name, test) {
    try {
      test();
      console.log(`ok - ${name}`);
    } catch (error) {
      failures += 1;
      console.error(`FAIL - ${name}`);
      console.error(error);
    }
  }

  function finish() {
    if (failures) {
      console.error(`\n${failures} ${label} test(s) failed.`);
      process.exitCode = 1;
      return false;
    }
    console.log(`\nAll ${label} tests passed.`);
    return true;
  }

  return Object.freeze({ finish, run, runSync });
}

module.exports = { createTestHarness };
