import * as ethers from "ethers";
import { Mnemonic, Wallet } from "ethers";

const RPC_URL = "https://virtual.mainnet.rpc.tenderly.co/49545acc-d00e-46de-908b-ef173bb182d6";
const EXPLORER_BASE_URL = "https://virtual.mainnet.rpc.tenderly.co/5bf9b9d4-f717-4657-af63-de185fe6477a";

const provider = new ethers.JsonRpcProvider(RPC_URL);
const signer = Wallet.fromPhrase(Mnemonic.fromEntropy(ethers.randomBytes(24)).phrase, provider);

(async () => {
  await provider.send("tenderly_setBalance", [
    signer.address,
    "0xDE0B6B3A7640000",
  ]);

  const tx = await signer.sendTransaction({
    to: "0xa5cc3c03994DB5b0d9A5eEdD10CabaB0813678AC",
    value: ethers.parseEther("0.01"),
  });

  console.log(`${EXPLORER_BASE_URL}/tx/${tx.hash}`);
})().catch(e => {
  console.error(e);
  process.exitCode = 1;
});