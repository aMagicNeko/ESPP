import { ethers } from "hardhat";

async function main() {
    // This will automatically check for the named contract in the `contracts` folder
    const MyContract = await ethers.getContractFactory("MyArb");

    // Here we deploy the contract with an initial value set for 'myValue'
    const myContract = await MyContract.deploy(); // You can replace 123 with any initial value

    // The contract is mined and deployed to the network
    //await myContract.deploy();

    console.log("MyContract deployed to:", myContract.target);
}

main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
});