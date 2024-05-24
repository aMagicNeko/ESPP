import { HardhatUserConfig } from "hardhat/config";
import "@nomicfoundation/hardhat-toolbox";
import * as tenderly from "@tenderly/hardhat-tenderly";

tenderly.setup({ automaticVerifications: true });

const config: HardhatUserConfig = {
  solidity: {
    compilers: [
      {
        version: "0.8.24",
        settings: {
          optimizer: {
            enabled: true,
            runs: 10
          },
          viaIR: true
        }
      }
      ]
    },
  networks: {
    virtualMainnet: {
      
      url: "https://virtual.mainnet.rpc.tenderly.co/e921ecee-21bc-43c7-8eb2-1b089391ad4d",
    },
  },
  tenderly: {
    // https://docs.tenderly.co/account/projects/account-project-slug
    project: "project",
    username: "ekko",
  },
};

export default config;