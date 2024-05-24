// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;
interface IERC20 {
    function balanceOf(address account) external view returns (uint);
    function transfer(address recipient, uint amount) external returns (bool);
}

interface IWETH is IERC20 {
    function deposit() external payable;
    function withdraw(uint amount) external;
}

interface IUniswapV3Pool {
    function swap(
        address recipient,
        bool zeroForOne,
        int256 amountSpecified,
        uint160 sqrtPriceLimitX96,
        bytes calldata data
    ) external returns (int256 amount0, int256 amount1);
}

interface IUniswapV2Pair {
    function swap(uint amount0Out, uint amount1Out, address to, bytes calldata data) external;
    function getReserves() external view returns (uint112 reserve0, uint112 reserve1, uint32 blockTimestampLast);
}

contract MyArb {
    address public owner;
    struct Step {
        address poolAddress;
        uint stepType; // 0 for v2, 1 for v3 | 2 for ZeroForOne
        address token0;
        address token1;
    }
    constructor() {
        owner = msg.sender;
    }

    modifier onlyOwner() {
        require(msg.sender == owner, "o");
        _;
    }

    function uniswapV2Call(address sender, uint amount0, uint amount1, bytes calldata data) external {
        (
            address token0,
            address token1,
            //bool flash,
            bytes memory stepsData,
            uint amountIn
        ) = abi.decode(data, (address, address, bytes, uint));
        
        //if (flash) {
            Step[] memory steps = abi.decode(stepsData, (Step[]));
            impl(steps, amount0 > 0 ? amount0 : amount1);
        //}
        // Repay the borrowed tokens plus fee
        if (amount0 > 0) {
            safeTransfer(token1, msg.sender, amountIn);
            //IERC20(token1).transfer(msg.sender, amountIn);
        }
        if (amount1 > 0) {
            safeTransfer(token0, msg.sender, amountIn);
            //IERC20(token0).transfer(msg.sender, amountIn);
        }
    }

    /// @dev The minimum value that can be returned from #getSqrtRatioAtTick. Equivalent to getSqrtRatioAtTick(MIN_TICK)
    uint160 internal constant MIN_SQRT_RATIO = 4295128739 + 1;
    /// @dev The maximum value that can be returned from #getSqrtRatioAtTick. Equivalent to getSqrtRatioAtTick(MAX_TICK)
    uint160 internal constant MAX_SQRT_RATIO = 1461446703485210103287273052203988822378723970342 - 1;

    function uniswapV3SwapCallback(
        int256 amount0Delta,
        int256 amount1Delta,
        bytes calldata data
    ) external {        
        (
            address token0,
            address token1,
            bool flash,
            bytes memory stepsData
        ) = abi.decode(data, (address, address, bool, bytes));
        
        if (flash) {
            Step[] memory steps = abi.decode(stepsData, (Step[]));
            impl(steps, uint(amount0Delta < 0 ? -amount0Delta : -amount1Delta));
        }
        // Repay the borrowed tokens
        if (amount0Delta > 0) {
            safeTransfer(token0, msg.sender, uint(amount0Delta));
            //IERC20(token0).transfer(msg.sender, uint(amount0Delta));
        }
        if (amount1Delta > 0) {
            safeTransfer(token1, msg.sender, uint(amount1Delta));
            //IERC20(token1).transfer(msg.sender, uint(amount1Delta));
        }
    }

    function impl(Step[] memory steps, uint prevAmount) internal {
        for (uint i = 0; i < steps.length; i++) {
            Step memory step = steps[i];
            bool zeroForOne = (step.stepType & 2) != 0;
            if ((step.stepType & 1) == 0) {
                // V2
                (uint reserve0, uint reserve1, ) = IUniswapV2Pair(step.poolAddress).getReserves();
                uint amount0Out = 0;
                uint amount1Out = 0;
                uint amountIn = prevAmount;
                if (zeroForOne) {
                    if (amountIn == 0) {
                        amountIn = IERC20(step.token0).balanceOf(address(this));
                    }
                    amount1Out = getAmountOut(amountIn, reserve0, reserve1);
                    safeTransfer(step.token0, step.poolAddress, amountIn);
                    //IERC20(step.token0).transfer(step.poolAddress, amountIn);
                }
                else {
                    if (amountIn == 0) {
                        amountIn = IERC20(step.token1).balanceOf(address(this));
                    }
                    amount0Out = getAmountOut(amountIn, reserve1, reserve0);
                    safeTransfer(step.token1, step.poolAddress, amountIn);
                    //IERC20(step.token1).transfer(step.poolAddress, amountIn);
                }
                prevAmount = 0;
                IUniswapV2Pair(step.poolAddress).swap(amount0Out, amount1Out, address(this), "");
            } else {
                // V3
                int256 amountSpecified = int(prevAmount);
                if (amountSpecified == 0) {
                    amountSpecified = zeroForOne ? int256(IERC20(step.token0).balanceOf(address(this))) : int256(IERC20(step.token1).balanceOf(address(this)));
                }
                uint160 limitSqrtPrice = zeroForOne ? MIN_SQRT_RATIO : MAX_SQRT_RATIO;
                (int amount1, int amount2) = IUniswapV3Pool(step.poolAddress).swap(address(this), zeroForOne, amountSpecified, limitSqrtPrice, abi.encode(step.token0, step.token1, false, ""));
                prevAmount = zeroForOne ? uint(-amount2) : uint(-amount1);
            }
        }
    }
    // @input: num of input token Of FirstPool
    function startFlashLoan(Step calldata initStep, Step[] calldata steps, uint256 input, uint256 fees, bytes calldata endStepData) external {
        uint balanceBefore = address(this).balance;
        uint stepType = initStep.stepType;
        bytes memory stepsData = abi.encode(steps);
        bool zeroForOne = (stepType & 2) != 0;
        uint wethOut = 0;
        if ((stepType & 1) == 0) {
            // v2
            // v2
            (uint reserve0, uint reserve1, ) = IUniswapV2Pair(initStep.poolAddress).getReserves();
            uint amount0Out = 0;
            uint amount1Out = 0;
            if (zeroForOne) {
                amount1Out = getAmountOut(input, reserve0, reserve1);
            }
            else {
                amount0Out = getAmountOut(input, reserve1, reserve0);
            }
            IUniswapV2Pair(initStep.poolAddress).swap(amount0Out, amount1Out, address(this), abi.encode(initStep.token0, initStep.token1, stepsData, input));
        }
        else {
            // v3
            uint160 limitSqrtPrice = zeroForOne ? MIN_SQRT_RATIO : MAX_SQRT_RATIO;
            (int amount0Out, int amount1Out) = IUniswapV3Pool(initStep.poolAddress).swap(address(this), zeroForOne, int256(input), limitSqrtPrice, abi.encode(initStep.token0, initStep.token1, true, stepsData));
            wethOut = zeroForOne ? uint(-amount1Out) : uint(-amount0Out);
        }
        // endstep
        if ((fees & 1) != 0) {
                (
                Step memory endStep
            ) = abi.decode(endStepData, (Step));
            bool zeroForOneLast = (endStep.stepType & 2) != 0;
            uint inputNum = wethOut;
            if (inputNum == 0) {
                inputNum = IERC20(zeroForOneLast ? endStep.token0 : endStep.token1).balanceOf(address(this));
            }
            if ((endStep.stepType & 1) == 0) {
                // v2
                (uint reserve0, uint reserve1, ) = IUniswapV2Pair(endStep.poolAddress).getReserves();
                uint amount0Out = 0;
                uint amount1Out = 0;
                if (zeroForOneLast) {
                    amount1Out = getAmountOut(inputNum, reserve0, reserve1);
                    safeTransfer(endStep.token0, endStep.poolAddress, inputNum);
                    //IERC20(endStep.token0).transfer(endStep.poolAddress, inputNum);
                }
                else {
                    amount0Out = getAmountOut(inputNum, reserve1, reserve0);
                    safeTransfer(endStep.token1, endStep.poolAddress, inputNum);
                    //IERC20(endStep.token1).transfer(endStep.poolAddress, inputNum);
                }
                IUniswapV2Pair(initStep.poolAddress).swap(amount0Out, amount1Out, address(this), "");
                wethOut = 0;
            }
            else {
                // v3
                uint160 limitSqrtPrice = zeroForOneLast ? MIN_SQRT_RATIO : MAX_SQRT_RATIO;
                (int amount0Out, int amount1Out) = IUniswapV3Pool(initStep.poolAddress).swap(address(this), zeroForOneLast, int256(inputNum), limitSqrtPrice, abi.encode(initStep.token0, initStep.token1, true, stepsData));
                wethOut = zeroForOneLast ? uint(-amount1Out) : uint(-amount0Out);
            }
        }
        IWETH weth = IWETH(0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2);
        wethOut = weth.balanceOf(address(this));
        weth.withdraw(wethOut);

        uint bribe = fees >> 128;
        block.coinbase.transfer(bribe);

        uint gasFee = fees & ((1 << 128) - 1);
        require(balanceBefore + gasFee < address(this).balance, "Z");
    }

    function withdrawFunds() external onlyOwner {
        payable(owner).transfer(address(this).balance);
    }

    receive() external payable {}

    function getAmountOut(uint amountIn, uint reserveIn, uint reserveOut) public pure returns (uint amountOut) {
        uint amountInWithFee = amountIn * 997; // 扣除0.3%的手续费
        uint numerator = amountInWithFee * reserveOut;
        uint denominator = (reserveIn * 1000) + amountInWithFee;
        amountOut = numerator / denominator;
    }

    function safeTransfer(
        address token,
        address to,
        uint256 value
    ) internal {
        (bool success, bytes memory data) =
            token.call(abi.encodeWithSelector(0xa9059cbb, to, value));
        require(success && (data.length == 0 || abi.decode(data, (bool))), 'X');
    }
}