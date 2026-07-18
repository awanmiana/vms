const policies = require("../../shared/resilience");
const { ResiliencePolicyService } = require("./service");

module.exports = {
  ...policies,
  ResiliencePolicyService
};
