const { ethers } = require("hardhat"); // ethers.js library
let tokenMap = {};
let faken = BigInt("11579208923731619542357098500868790785326998466564056403945758400791312963995");
const log1 = `UniswapV3 pool:0x86d257cdb7bc9c0df10e84c8709697f92770b335 tickSpace:60 fee:3000 token1:1 token2:29 tick:-127588 sqrt_price:134430725175035685688384166 liqudity:79886125713661910
`; 

const log2 = `UniswapV2 pool:0xc5be99a02c6857f9eac67bbce58df5572498f40c token1:1 token2:29 reserve0:1585580924268879417777 reserve1:4265012534954539`;

const log3 = `UniswapV3 pool:0xc7bbec68d12a0d1830360f8ec58fa599ba1b0e9b tickSpace:1 fee:100 token1:1 token2:56 tick:-193939 sqrt_price:4872773211248220066259075 liqudity:169872449170087596 
-194186 -16037485469191368 -194079 -8017267496262492 -194021 -870583342667379 -193893 -408514569376740 -193827 -90831761854641126 -193633 -66249539495303483`;

const log4 = `UniswapV2 pool:0x0d4a11d5eeaac28ec3f61d100daf4d40471f1852 token1:1 token2:56 reserve0:15246391132629389797321 reserve1:57686081566809`;


// Function to parse the log and return pool details
describe("MyArb",  function () {
    it("start to test uniswapV4", async function () {    
        const [owner, user1] = await ethers.getSigners(); // Get signers
const ERC20 = await ethers.getContractFactory("ERC20"); // ERC20 contract factory
const WETH9 = await ethers.getContractFactory("WETH9"); // WETH9 contract factory
const UniswapV2Pair = await ethers.getContractFactory("UniswapV2Pair"); // UniswapV2Pair contract factory
const UniswapV3Pool = await ethers.getContractFactory("UniswapV3Pool"); // UniswapV3Pool contract factory

async function setBalance(address, amount) {
  // ethers.utils.parseEther converts an ETH value into Wei

  await network.provider.send("hardhat_setBalance", [
      address,
      amount
  ]);
}

async function parseLog(log) {
    async function getTokenAddress(index) {
        if (!tokenMap[index]) {
            if (index == 1) {
                const weth = await WETH9.deploy();
                await setBalance(weth.target, "0x9999999999999999999999999"); // Set balance
                tokenMap[index] = weth;
            } else {
                const token = await ERC20.deploy();
                tokenMap[index] = token;
            }
        }
        return tokenMap[index].target;
      };
  const details = {};
  const lines = log.trim().split("\n");
  const firstLine = lines[0];
  if (firstLine.startsWith("UniswapV3 pool:")) {
    details.type = "UniswapV3";
    const [_, pool, tickSpace, fee, token1, token2, tick, sqrt_price, liquidity] = firstLine.match(/UniswapV3 pool:([\w\d]+) tickSpace:(\d+) fee:(\d+) token1:(\d+) token2:(\d+) tick:(-?\d+) sqrt_price:(\d+) liqudity:(\d+)/);
    details.pool = pool;
    details.tickSpace = parseInt(tickSpace);
    details.fee = parseInt(fee);
    details.token1 = token1;
    details.token2 = token2;
    details.tick = parseInt(tick);
    details.sqrt_price = BigInt(sqrt_price);
    details.liquidity = BigInt(liquidity);

    details.token1Address = await getTokenAddress(details.token1);
    details.token2Address = await getTokenAddress(details.token2);


    details.liquidityNets = lines.slice(1).flatMap(line => {
        return line.split(" ").map((_, i, arr) => {
          if (i % 2 === 0) {
            return { tick: parseInt(arr[i]), liquidityNet: BigInt(arr[i + 1]) };
          }
          return [];
        }).filter(item => item.tick !== undefined);
      });
  } else if (firstLine.startsWith("UniswapV2 pool:")) {
    details.type = "UniswapV2";
    const [_, pool, token1, token2, reserve0, reserve1] = firstLine.match(/UniswapV2 pool:([\w\d]+) token1:(\d+) token2:(\d+) reserve0:(\d+) reserve1:(\d+)/);
    details.pool = pool;
    details.token1 = token1;
    details.token2 = token2;
    details.reserve0 = BigInt(reserve0);
    details.reserve1 = BigInt(reserve1);
    details.token1Address = await getTokenAddress(details.token1);
    details.token2Address = await getTokenAddress(details.token2);
  }

  return details;
}


async function deployAndSetupPool(details) {
    if (details.type === "UniswapV3") {
      const { pool, tickSpace, fee, token1Address, token2Address, tick, sqrt_price, liquidity, liquidityNets } = details;
      const UniswapV3PoolInstance = await UniswapV3Pool.deploy(token1Address, token2Address, fee, tick, tickSpace, sqrt_price, liquidity);
      for (const { tick, liquidityNet } of liquidityNets) {
        await UniswapV3PoolInstance.setLiquidityNet(tick, liquidityNet);
      }
      await tokenMap[details.token1].update(UniswapV3PoolInstance.target, faken);
      await tokenMap[details.token2].update(UniswapV3PoolInstance.target, faken);
      console.log(`UniswapV3 Pool deployed at: ${UniswapV3PoolInstance.target}`);
      return UniswapV3PoolInstance;
    } else if (details.type === "UniswapV2") {
      const { pool, token1Address, token2Address, reserve0, reserve1 } = details;
      const UniswapV2PairInstance = await UniswapV2Pair.deploy(token1Address, token2Address, reserve0, reserve1);
      await tokenMap[details.token1].update(UniswapV2PairInstance.target, reserve0);
      await tokenMap[details.token2].update(UniswapV2PairInstance.target, reserve1);
      console.log(`UniswapV2 Pool deployed at: ${UniswapV2PairInstance.target}`);
      return UniswapV2PairInstance;
    }
  }



poolDetails1 = await parseLog(log1);
console.log(poolDetails1)
pool1 = await deployAndSetupPool(poolDetails1);

poolDetails2 = await parseLog(log2);
console.log(poolDetails2)
pool2 = await deployAndSetupPool(poolDetails2);

poolDetails3 = await parseLog(log3);
console.log(poolDetails3)
pool3 = await deployAndSetupPool(poolDetails3);

poolDetails4 = await parseLog(log4);
console.log(poolDetails4)
pool4 = await deployAndSetupPool(poolDetails4);


const MyArb = await ethers.getContractFactory("MyArb");
    const myArb = await MyArb.deploy();
    //Step calldata initStep, Step[] calldata steps, uint256 input, uint256 fees, bytes calldata endStepData
    const initStep = {
      poolAddress: pool1.target,
      stepType: 3, // 0 for v2, 1 for v3 | 2 for ZeroForOne
      token0: tokenMap[poolDetails1.token1].target,
      token1: tokenMap[poolDetails1.token2].target
  };
    const steps = [ {
      poolAddress: pool2.target,
      stepType: 0,
      token0: tokenMap[poolDetails2.token1].target,
      token1: tokenMap[poolDetails2.token2].target,
    }, {
      poolAddress: pool3.target,
      stepType: 3,
      token0: tokenMap[poolDetails3.token1].target,
      token1: tokenMap[poolDetails3.token2].target,
    }, 
    {
      poolAddress: pool4.target,
      stepType: 0,
      token0: tokenMap[poolDetails4.token1].target,
      token1: tokenMap[poolDetails4.token2].target
    }];
    const input = BigInt("0x127e4c4ddff22619");
    const gasfee = 0;//BigInt("1103758969") * BigInt("1200000000");
    
    const fees = BigInt("0x00000000000000000046595105ba45d30000000000000000001061bec80c99c0");
    //const fees = 0;
    endStepData = "0x";
    console.log(initStep);
    console.log(steps);
    
    result = await myArb.startFlashLoan(initStep, steps, input, fees, endStepData);
    //result = await myArb.test(initStep);
    console.log(result)
    // 检查套利后的余额
    const balance = await ethers.provider.getBalance(myArb.target);
    console.log('balance', balance);

});
});