const { expect } = require("chai");
const { ethers } = require("hardhat");
let faken = BigInt("9999999999999999999999999999");
/// @dev The minimum value that can be returned from #getSqrtRatioAtTick. Equivalent to getSqrtRatioAtTick(MIN_TICK)
let MIN_SQRT_RATIO = BigInt("4295128740");
    /// @dev The maximum value that can be returned from #getSqrtRatioAtTick. Equivalent to getSqrtRatioAtTick(MAX_TICK)
let MAX_SQRT_RATIO = BigInt("1461446703485210103287273052203988822378723970341");
/*
describe("TestUniswapV2", function () {
  /*
  let owner;
  let user1;
  let t1;
  let t2;
  let UniswapV2Pair1;
  let myaddress;
  it("start to test uniswapV2", async function () {
    [owner, user1] = await ethers.getSigners();
    const ERC20 = await ethers.getContractFactory("ERC20");
    const UniswapV2Pair = await ethers.getContractFactory("UniswapV2Pair")
    const UniswapV3Pool = await ethers.getContractFactory("UniswapV3Pool")
    myaddress = "0x05556053d0966c7701201a2103dcb56cf75bdd92";
    t1 = await ERC20.deploy();
    t2 = await ERC20.deploy();
    let reserve0 = BigInt("161033368823919330945");
    let reserve1 = BigInt("30032159742008568561006083");
    console.log("t1:", t1);
    console.log("t2:", t2);
    UniswapV2Pair1 = await UniswapV2Pair.deploy(t1.target, t2.target, reserve0, reserve1);
    await t1.update(UniswapV2Pair1.target, reserve0);
    await t2.update(UniswapV2Pair1.target, reserve1);
    let input = BigInt("8009326124424363185");
    let output = BigInt("1418871381540237309642075");
    await t1.update(myaddress, faken);
    await t2.update(myaddress, faken);
    let balanceT1MyAddress = await t1.balanceOf(myaddress);
    let balanceT2MyAddress = await t2.balanceOf(myaddress);
    console.log(`Balance of T1 in MyAddress: ${balanceT1MyAddress.toString()}`);
    console.log(`Balance of T2 in MyAddress: ${balanceT2MyAddress.toString()}`);
    balanceT1MyAddress = await t1.balanceOf(UniswapV2Pair1.target);
    balanceT2MyAddress = await t2.balanceOf(UniswapV2Pair1.target);
    console.log(`Balance of T1 in MyAddress: ${balanceT1MyAddress.toString()}`);
    console.log(`Balance of T2 in MyAddress: ${balanceT2MyAddress.toString()}`);
    // 付钱
    await t1.update(UniswapV2Pair1.target, input + reserve0);
    await UniswapV2Pair1.swap(0, output, myaddress, "0x");
    // 调用getReserves函数并获取返回值
    const result = await UniswapV2Pair1.getReserves();
    
    // 打印返回值
    console.log(`Reserve 0: ${result._reserve0}`);
    console.log(`Reserve 1: ${result._reserve1}`);
    console.log(`Block timestamp last: ${result._blockTimestampLast}`);
  });
});

  describe("TestUniswapV3", function () {
    let owner;
    let user1;
    let t1;
    let t2;
    let UniswapV3Pool1;
    let myaddress;
    it("start to test uniswapV2", async function () {
      [owner, user1] = await ethers.getSigners();
      const ERC20 = await ethers.getContractFactory("ERC20");
      const UniswapV3Pool = await ethers.getContractFactory("UniswapV3Pool")
    
      myaddress = "0x05556053d0966c7701201a2103dcb56cf75bdd92";
      t1 = await ERC20.deploy();
      t2 = await ERC20.deploy();
      await t1.update(myaddress, faken);
      await t2.update(myaddress, faken);
      let sqrtPrice = BigInt("329000514750533438388766539482033075");
      let liquidity = BigInt("338270972477820533");
      let netLiq1 = BigInt("338270972477820533");
      let tick1 = 296000;
      let netLiq2 = BigInt("-338270972477820533");
      let tick2 = 304800;
      let fee = 10000;
      let tick = 304799;
      let tickspace = 200;
      UniswapV3Pool1 = await UniswapV3Pool.deploy(faken, faken, t1.target, t2.target, fee, tick, tickspace, sqrtPrice, liquidity);
      await UniswapV3Pool1.setLiquidityNet(tick1, netLiq1);
      await UniswapV3Pool1.setLiquidityNet(tick2, netLiq2);

      await t1.update(UniswapV3Pool1.target, faken);
      await t2.update(UniswapV3Pool1.target, faken);
      result1 = await UniswapV3Pool1.getInfo();
      console.log(result1);
      let input = BigInt("45475851527");
      //let output = BigInt("31");
      let zeroForOne = true;
      let limitSqrtPrice = zeroForOne ? MIN_SQRT_RATIO : MAX_SQRT_RATIO;
      // 付钱
      const result = await UniswapV3Pool1.swap(myaddress, zeroForOne, input, limitSqrtPrice, "0x");
      result1 = await UniswapV3Pool1.getLastRet();
      console.log(result1);
      // 打印返回值
      console.log(result);
      //console.log(`amount0 1: ${amount1}`);
      result1 = await UniswapV3Pool1.getInfo();
      console.log(result1);
      result1 = await UniswapV3Pool1.getLastRet()
      console.log(result1);

      result1 = await UniswapV3Pool1.getSqrtRatioAtTick(304800);
      console.log(result1);

      result1 = await UniswapV3Pool1.getAmount0Delta(BigInt("329000514750533438388766539482033075"), BigInt("329000514879500807563583747329060302"), BigInt("338270972477820533"), 0);
      console.log(result1);
      result1 = await UniswapV3Pool1.getAmount1Delta(BigInt("329000514750533438388766539482033075"), BigInt("329000514879500807563583747329060302"), BigInt("338270972477820533"), 1);
      console.log(result1);
    });});
*/


async function setBalance(address, amount) {
  // ethers.utils.parseEther converts an ETH value into Wei

  await network.provider.send("hardhat_setBalance", [
      address,
      amount
  ]);
}
/*
  describe("MyArb",  function () {
    it("start to test uniswap", async function () {
    const UniswapV2Pair = await ethers.getContractFactory("UniswapV2Pair");
    const UniswapV3Pool = await ethers.getContractFactory("UniswapV3Pool");
    const WETH9 = await ethers.getContractFactory("WETH9");
    [owner, user1] = await ethers.getSigners();
    const ERC20 = await ethers.getContractFactory("ERC20");
    const t1 = await WETH9.deploy(); // 1
    const t2 = await ERC20.deploy(); // 29
    const t3 = await ERC20.deploy(); // 56
    
    await setBalance(t1.target, "0x9999999999999999999999999");

    const UniswapV3Pool1 = await UniswapV3Pool.deploy(t1.target, t2.target, 3000, -127588, 60, BigInt("29835945292897407815147100"), BigInt("79886125713661910"));
    //await UniswapV3Pool1.setLiquidityNet(-157696, BigInt("340282366920938463463374607431768211455"));
    //await UniswapV3Pool1.setLiquidityNet(-111676, BigInt("340282366920938463463374607431768211455"));
    const UniswapV2Pair2 = await UniswapV2Pair.deploy(t2.target, t3.target, 105557584377880, 97754);
    const UniswapV3Pool3 = await UniswapV3Pool.deploy(t1.target, t3.target, 3000, -196024, 60, BigInt("4390248520164088075219580"), BigInt("7345832694699330989"));

    await t1.update(UniswapV3Pool1.target, faken);
    await t2.update(UniswapV3Pool1.target, faken);
    await t1.update(UniswapV3Pool3.target, faken);
    await t3.update(UniswapV3Pool1.target, faken);
    await t2.update(UniswapV2Pair2, 105557584377880);
    await t3.update(UniswapV2Pair2, 97754);


//input num:1254120315681701 eth_out:47824185201709
//UniswapV3 pool:0x86d257cdb7bc9c0df10e84c8709697f92770b335 tickSpace:60 fee:3000 token1:1 token2:29 tick:-127588 sqrt_price:29835945292897407815147100 liqudity:79886125713661910 liquidity_net:-157696 340282366920938463463374607431768211455 -111676 -340282366920938463463374607431768211455 
//UniswapV2 pool:0x83503be303ff0e05a5d6dcd1c2a3bd589fb0ded4 token1:29 token2:56 reserve0:105557584377880 reserve1:97754
//UniswapV3 pool:0x4e68ccd3e89f51c3074ca5072bbac773960dfa36 tickSpace:60 fee:3000 token1:1 token2:56 tick:-196024 sqrt_price:4390248520164088075219580 liqudity:7345832694699330989 liquidity_net:-219136 115792089237316195423570985008687907853269984665640564039457584007913129639935 -173116 -115792089237316195423570985008687907853269984665640564039457584007913129639935 
   // 部署 MyArb 合约
    const MyArb = await ethers.getContractFactory("MyArb");
    const myArb = await MyArb.deploy();
    //Step calldata initStep, Step[] calldata steps, uint256 input, uint256 fees, bytes calldata endStepData
    const initStep = {
      poolAddress: UniswapV3Pool1.target,
      stepType: 3, // 0 for v2, 1 for v3 | 2 for ZeroForOne
      token0: t1.target,
      token1: t2.target
  };
    const steps = [ {
      poolAddress: UniswapV2Pair2.target,
      stepType: 2,
      token0: t2.target,
      token1: t3.target
    }, {
      poolAddress: UniswapV3Pool3.target,
      stepType: 1,
      token0: t1.target,
      token1: t3.target
    }];
    const input = BigInt("1254120315681701");
    const gasfee = BigInt("9564837040340");
    const fees = BigInt("3254745387299958971648291206136383228155702322135040") + gasfee; 
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
/
describe("Balance manipulation", function () {
    it("should set the balance of an address", async function () {
        const [owner] = await ethers.getSigners();
        const address = owner.address;
        
        await setBalance(address, "0x999999999999999999999999990");

        // Check balance
        const balance = await ethers.provider.getBalance(address);
        console.log(`Balance is now: ${balance} ETH`);
    });
});
*/
/*
  describe("MyArb",  function () {
    it("start to test uniswapV2", async function () {
    const UniswapV2Pair = await ethers.getContractFactory("UniswapV2Pair");
    const UniswapV3Pool = await ethers.getContractFactory("UniswapV3Pool");
    const WETH9 = await ethers.getContractFactory("WETH9");
    [owner, user1] = await ethers.getSigners();
    const ERC20 = await ethers.getContractFactory("ERC20");
    const t1 = await WETH9.deploy(); // 1
    const t2 = await ERC20.deploy(); // 81520
    const t3 = await ERC20.deploy(); // 270864
    await setBalance(t1.target, "0x9999999999999999999999999");
    const UniswapV3Pool1 = await UniswapV3Pool.deploy(t1.target, t2.target, 10000, 131199, 200, BigInt("55938259044895405291823287410080"), BigInt("236781235733546170885299"));
    await UniswapV3Pool1.setLiquidityNet(116200, BigInt("236781235733546170885299"));
    await UniswapV3Pool1.setLiquidityNet(131200, BigInt("-236781235733546170885299"));
    const UniswapV3Pool2 = await UniswapV3Pool.deploy(t1.target, t2.target, 10000, -96316, 200, BigInt("641961206496414902538480165"), BigInt("130478251499099075"));
    const UniswapV3Pool3 = await UniswapV3Pool.deploy(t3.target, t1.target, 10000, -117322, 200, BigInt("1323357755427813870601529158"), BigInt("1950783006148795540644"));
    const UniswapV3Pool4 = await UniswapV3Pool.deploy(t3.target, t1.target, 3000, -117437, 60, BigInt("222081582718816958029316477"), BigInt("290579590233324214537632"));
  
  
    await t1.update(UniswapV3Pool1.target, faken);
    await t2.update(UniswapV3Pool1.target, faken);
    await t1.update(UniswapV3Pool2.target, faken);
    await t2.update(UniswapV3Pool2.target, faken);
    await t3.update(UniswapV3Pool3, faken);
    await t1.update(UniswapV3Pool3, faken);
    await t3.update(UniswapV3Pool4, faken);
    await t1.update(UniswapV3Pool4, faken);
  
  
  //input num:1254120315681701 eth_out:47824185201709
  //UniswapV3 pool:0x86d257cdb7bc9c0df10e84c8709697f92770b335 tickSpace:60 fee:3000 token1:1 token2:29 tick:-127588 sqrt_price:29835945292897407815147100 liqudity:79886125713661910 liquidity_net:-157696 340282366920938463463374607431768211455 -111676 -340282366920938463463374607431768211455 
  //UniswapV2 pool:0x83503be303ff0e05a5d6dcd1c2a3bd589fb0ded4 token1:29 token2:56 reserve0:105557584377880 reserve1:97754
  //UniswapV3 pool:0x4e68ccd3e89f51c3074ca5072bbac773960dfa36 tickSpace:60 fee:3000 token1:1 token2:56 tick:-196024 sqrt_price:4390248520164088075219580 liqudity:7345832694699330989 liquidity_net:-219136 115792089237316195423570985008687907853269984665640564039457584007913129639935 -173116 -115792089237316195423570985008687907853269984665640564039457584007913129639935 
   // 部署 MyArb 合约
    const MyArb = await ethers.getContractFactory("MyArb");
    const myArb = await MyArb.deploy();
    //Step calldata initStep, Step[] calldata steps, uint256 input, uint256 fees, bytes calldata endStepData
    const initStep = {
      poolAddress: UniswapV3Pool1.target,
      stepType: 3, // 0 for v2, 1 for v3 | 2 for ZeroForOne
      token0: t1.target,
      token1: t2.target
  };
    const steps = [ {
      poolAddress: UniswapV3Pool2.target,
      stepType: 1,
      token0: t1.target,
      token1: t2.target
    }, {
      poolAddress: UniswapV3Pool3.target,
      stepType: 1,
      token0: t3.target,
      token1: t1.target
    }, 
    {
      poolAddress: UniswapV3Pool4.target,
      stepType: 3,
      token0: t3.target,
      token1: t1.target
    }];
    const input = BigInt("1352015894");
    const gasfee = BigInt("0");
    //const fees = BigInt("306293332639640573003443769349771756492752557471340429312") + gasfee; 
    const fees = 0;
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
  */

  describe("MyArb",  function () {
    it("start to test uniswapV4", async function () {
    const UniswapV2Pair = await ethers.getContractFactory("UniswapV2Pair");
    const UniswapV3Pool = await ethers.getContractFactory("UniswapV3Pool");
    const WETH9 = await ethers.getContractFactory("WETH9");
    [owner, user1] = await ethers.getSigners();
    const ERC20 = await ethers.getContractFactory("ERC20");
    const t1 = await WETH9.deploy(); // 1
    const t2 = await ERC20.deploy(); // 56
    const t3 = await ERC20.deploy(); // 0
    //UniswapV3 pool:0xc7bbec68d12a0d1830360f8ec58fa599ba1b0e9b tickSpace:1 fee:100 token1:1 token2:56 tick:-196224 sqrt_price:4333759207957309416756099 liqudity:295585733865034257
    //-196224 1010415561354318 -196094 -78195380914566 -196059 -24448431016597321 -196026 520202268528394 -195833 729663811259263 -195666 -106925946128513 -195611 -371549156461246 -195587 -4293502247362936
    await setBalance(t1.target, "0x9999999999999999999999999");
    const UniswapV3Pool1 = await UniswapV3Pool.deploy(t1.target, t2.target, 100, 131199, 1, BigInt("4333759207957309416756099"), BigInt("295585733865034257"));
    await UniswapV3Pool1.setLiquidityNet(-196224, BigInt("1010415561354318"));
    await UniswapV3Pool1.setLiquidityNet(-196094, BigInt("-78195380914566"));
    await UniswapV3Pool1.setLiquidityNet(-196059, BigInt("-24448431016597321"));
    await UniswapV3Pool1.setLiquidityNet(-196026, BigInt("-520202268528394"));
    await UniswapV3Pool1.setLiquidityNet(-195833, BigInt("-729663811259263"));
    await UniswapV3Pool1.setLiquidityNet(-195666, BigInt("-106925946128513"));
    await UniswapV3Pool1.setLiquidityNet(-195611, BigInt("-371549156461246"));
    await UniswapV3Pool1.setLiquidityNet(-195587, BigInt("-4293502247362936"));
    //UniswapV3 pool:0x3416cf6c708da44db2624d63ea0aaef7113527c6 tickSpace:1 fee:100 token1:0 token2:56 tick:-1 sqrt_price:79226700502522481852630465528 liqudity:141245734580690848
    //-102 204004711968 -100 89179463 -98 7691293177 -77 2009741 -52 3934979497517 -50 2996378655406 -49 392989184 -44 44108387655 -41 6695618837 -38 3494709585741 -36 21329472506 -34 213025742309305 -31 5876297144 -29 3713267354 -27 512682451 -25 362020916610654 -24 15870576204876 -23 7278047924526 -22 22046342106058 -21 51718224888376 -20 854368935069 -19 22923725257320 -18 82137294128 -17 90092892355511 -16 113198968446301 -15 1239845746374562 -14 165926401125761 -13 1428967282406981 -12 821465165294669 -11 1979348094433977 -10 1284400178647950 -9 99599574297857 -8 443902004013629 -7 1384380041497249 -6 3221258784802320 -5 1958553414648110 -4 721168859695328 -3 6785305330653117 -2 235720634589226 -1 118566014045749446 0 -115886645989144541 1 -1387782015167347 2 -1899096587329146 3 -6181296413232774 4 -662922402163989 5 -2325446719357062 6 -349976409348186 7 -5472210175038452 8 -1152443444707111 9 -2149539964741802 10 -222769320034990 11 -1895191950681028 12 -307811064686397 13 -764110320221 14 -198780544927852 15 -15370858395940 16 -16261059919475 17 -7489296068936 18 -22358209464990 19 -18757493482151 20 -183126852456426 21 -55073833528 22 -129221995290075 23 -32980683629740 24 -413023590605547 25 -213568437565034 26 -27269027603 29 -44644095397060 30 -51129164047330 33 -797024325197 37 -44108387655 49 -3934979497517 52 -392989184 58 -39450004 95 -20032 99 -204004711968 100 -7780472640
    const UniswapV3Pool2 = await UniswapV3Pool.deploy(t3.target, t2.target, 100, -1, 1, BigInt("79226700502522481852630465528"), BigInt("141245734580690848"));
    await UniswapV3Pool2.setLiquidityNet(-102, BigInt("204004711968"));
    await UniswapV3Pool2.setLiquidityNet(-100, BigInt("89179463"));
    
    await UniswapV3Pool2.setLiquidityNet(-98, BigInt("7691293177"));
    await UniswapV3Pool2.setLiquidityNet(-77, BigInt("2009741"));
    await UniswapV3Pool2.setLiquidityNet(-52, BigInt("3934979497517"));
    await UniswapV3Pool2.setLiquidityNet(-50, BigInt("2996378655406"));
    await UniswapV3Pool2.setLiquidityNet(-49, BigInt("392989184"));
    await UniswapV3Pool2.setLiquidityNet(-44, BigInt("44108387655"));
    await UniswapV3Pool2.setLiquidityNet(-41, BigInt("6695618837"));
    //-38 3494709585741 -36 21329472506 -34 213025742309305 -31 5876297144 -29 3713267354 -27 512682451 -25 362020916610654 -24 15870576204876 -23 7278047924526 
    //-22 22046342106058 -21 51718224888376 -20 854368935069 -19 22923725257320 -18 82137294128 -17 90092892355511 -16 113198968446301 -15 1239845746374562 -14 165926401125761 -13 1428967282406981 -12 821465165294669 -11 1979348094433977 -10 1284400178647950 -9 99599574297857 -8 443902004013629 -7 1384380041497249 -6 3221258784802320 -5 1958553414648110 -4 721168859695328 -3 6785305330653117 -2 235720634589226 -1 118566014045749446 0 -115886645989144541 1 -1387782015167347 2 -1899096587329146 3 -6181296413232774 4 -662922402163989 5 -2325446719357062 6 -349976409348186 7 -5472210175038452 8 -1152443444707111 9 -2149539964741802 10 -222769320034990 11 -1895191950681028 12 -307811064686397 13 -764110320221 14 -198780544927852 15 -15370858395940 16 -16261059919475 17 -7489296068936 18 -22358209464990 19 -18757493482151 20 -183126852456426 21 -55073833528 22 -129221995290075 23 -32980683629740 24 -413023590605547 25 -213568437565034 26 -27269027603 29 -44644095397060 30 -51129164047330 33 -797024325197 37 -44108387655 49 -3934979497517 52 -392989184 58 -39450004 95 -20032 99 -204004711968 100 -7780472640
    await UniswapV3Pool2.setLiquidityNet(-38, BigInt("3494709585741"));
    await UniswapV3Pool2.setLiquidityNet(-36, BigInt("21329472506"));
    await UniswapV3Pool2.setLiquidityNet(-34, BigInt("213025742309305"));
    await UniswapV3Pool2.setLiquidityNet(-31, BigInt("5876297144"));
    await UniswapV3Pool2.setLiquidityNet(-29, BigInt("3713267354"));
    await UniswapV3Pool2.setLiquidityNet(-27, BigInt("512682451"));
    await UniswapV3Pool2.setLiquidityNet(-25, BigInt("362020916610654"));
    await UniswapV3Pool2.setLiquidityNet(-24, BigInt("15870576204876"));
    await UniswapV3Pool2.setLiquidityNet(-23, BigInt("7278047924526"));
    await UniswapV3Pool2.setLiquidityNet(-22, BigInt("22046342106058"));
    await UniswapV3Pool2.setLiquidityNet(-21, BigInt("51718224888376"));
    await UniswapV3Pool2.setLiquidityNet(-20, BigInt("854368935069"));
    await UniswapV3Pool2.setLiquidityNet(-19, BigInt("22923725257320"));
    await UniswapV3Pool2.setLiquidityNet(-18, BigInt("82137294128"));
    await UniswapV3Pool2.setLiquidityNet(-17, BigInt("90092892355511"));
    await UniswapV3Pool2.setLiquidityNet(-16, BigInt("113198968446301"));
    await UniswapV3Pool2.setLiquidityNet(-15, BigInt("1239845746374562"));

    const UniswapV3Pool3 = await UniswapV3Pool.deploy(t3.target, t1.target, 10000, -117322, 200, BigInt("1323357755427813870601529158"), BigInt("1950783006148795540644"));
    const UniswapV3Pool4 = await UniswapV3Pool.deploy(t3.target, t1.target, 3000, -117437, 60, BigInt("222081582718816958029316477"), BigInt("290579590233324214537632"));
  
  
    await t1.update(UniswapV3Pool1.target, faken);
    await t2.update(UniswapV3Pool1.target, faken);
    await t1.update(UniswapV3Pool2.target, faken);
    await t2.update(UniswapV3Pool2.target, faken);
    await t3.update(UniswapV3Pool3, faken);
    await t1.update(UniswapV3Pool3, faken);
    await t3.update(UniswapV3Pool4, faken);
    await t1.update(UniswapV3Pool4, faken);
  
  
  //input num:1254120315681701 eth_out:47824185201709
  //UniswapV3 pool:0x86d257cdb7bc9c0df10e84c8709697f92770b335 tickSpace:60 fee:3000 token1:1 token2:29 tick:-127588 sqrt_price:29835945292897407815147100 liqudity:79886125713661910 liquidity_net:-157696 340282366920938463463374607431768211455 -111676 -340282366920938463463374607431768211455 
  //UniswapV2 pool:0x83503be303ff0e05a5d6dcd1c2a3bd589fb0ded4 token1:29 token2:56 reserve0:105557584377880 reserve1:97754
  //UniswapV3 pool:0x4e68ccd3e89f51c3074ca5072bbac773960dfa36 tickSpace:60 fee:3000 token1:1 token2:56 tick:-196024 sqrt_price:4390248520164088075219580 liqudity:7345832694699330989 liquidity_net:-219136 115792089237316195423570985008687907853269984665640564039457584007913129639935 -173116 -115792089237316195423570985008687907853269984665640564039457584007913129639935 
   // 部署 MyArb 合约
    const MyArb = await ethers.getContractFactory("MyArb");
    const myArb = await MyArb.deploy();
    //Step calldata initStep, Step[] calldata steps, uint256 input, uint256 fees, bytes calldata endStepData
    const initStep = {
      poolAddress: UniswapV3Pool1.target,
      stepType: 3, // 0 for v2, 1 for v3 | 2 for ZeroForOne
      token0: t1.target,
      token1: t2.target
  };
    const steps = [ {
      poolAddress: UniswapV3Pool2.target,
      stepType: 1,
      token0: t1.target,
      token1: t2.target
    }, {
      poolAddress: UniswapV3Pool3.target,
      stepType: 1,
      token0: t3.target,
      token1: t1.target
    }, 
    {
      poolAddress: UniswapV3Pool4.target,
      stepType: 3,
      token0: t3.target,
      token1: t1.target
    }];
    const input = BigInt("1352015894");
    const gasfee = BigInt("0");
    //const fees = BigInt("306293332639640573003443769349771756492752557471340429312") + gasfee; 
    const fees = 0;
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



  