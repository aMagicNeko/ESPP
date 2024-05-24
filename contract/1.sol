// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

interface IERC20 {
    function totalSupply() external view returns (uint);

    function balanceOf(address account) external view returns (uint);

    function transfer(address recipient, uint amount) external returns (bool);

    function allowance(address owner, address spender) external view returns (uint);

    function approve(address spender, uint amount) external returns (bool);

    function transferFrom(
        address sender,
        address recipient,
        uint amount
    ) external returns (bool);

    event Transfer(address indexed from, address indexed to, uint value);
    event Approval(address indexed owner, address indexed spender, uint value);
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

    function flash(
        address recipient,
        uint256 amount0,
        uint256 amount1,
        bytes calldata data
    ) external;
}

interface IUniswapV2Pair {
    function swap(uint amount0Out, uint amount1Out, address to, bytes calldata data) external;
}

contract MyArb {
    address public owner;
    uint256 public startBalance;

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
        require(msg.sender == owner, "Caller is not the owner");
        _;
    }

    function uniswapV2Call(address sender, uint amount0, uint amount1, bytes calldata data) external {
        (
            address token0,
            address token1,
            bool flash,
            bytes memory stepsData
        ) = abi.decode(data, (address, address, bool, bytes));
        
        if (flash) {
            Step[] memory steps = abi.decode(stepsData, (Step[]));
            impl(steps);
        }

        // Repay the borrowed tokens plus fee
        if (amount0 > 0) {
            uint amount0Repay = amount0 + ((amount0 * 3) / 997) + 1;
            require(IERC20(token0).transfer(sender, amount0Repay), "Failed to repay token0");
        }
        if (amount1 > 0) {
            uint amount1Repay = amount1 + ((amount1 * 3) / 997) + 1;
            require(IERC20(token1).transfer(sender, amount1Repay), "Failed to repay token1");
        }
    }

    function uniswapV3SwapCallback(
        int256 amount0Delta,
        int256 amount1Delta,
        bytes calldata data
    ) external {        
        (
            address token0,
            address token1,
            bool flash,
            bool zeroForOne,
            bytes memory stepsData
        ) = abi.decode(data, (address, address, bool, bool, bytes));
        
        if (flash) {
            Step[] memory steps = abi.decode(stepsData, (Step[]));
            impl(steps);
        }

        // Repay the borrowed tokens
        if (zeroForOne) {
            IERC20(token0).transferFrom(msg.sender, address(this), uint(amount0Delta));
        } else {
            IERC20(token1).transferFrom(msg.sender, address(this), uint(amount1Delta));
        }
    }

    function impl(Step[] memory steps) internal {
        for (uint i = 0; i < steps.length; i++) {
            Step memory step = steps[i];
            if ((step.stepType & 1) == 0) {
                // V2
                uint amount0Out = ((step.stepType & 2) != 0) ? 0 : IERC20(step.token0).balanceOf(address(this));
                uint amount1Out = ((step.stepType & 2) != 0) ? IERC20(step.token1).balanceOf(address(this)) : 0;
                IUniswapV2Pair(step.poolAddress).swap(amount0Out, amount1Out, address(this), abi.encode(step.token0, step.token1, false, ""));
            } else {
                // V3
                bool zeroForOne = (step.stepType & 2) != 0;
                int256 amountSpecified = zeroForOne ? int256(IERC20(step.token0).balanceOf(address(this))) : int256(IERC20(step.token1).balanceOf(address(this)));
                IUniswapV3Pool(step.poolAddress).swap(address(this), zeroForOne, amountSpecified, 1 << 112, abi.encode(step.token0, step.token1, false, zeroForOne, ""));
            }
        }
    }

    function startFlashLoan(Step calldata initStep, Step[] calldata steps, uint256 input, uint256 fees) external {
        uint balanceBefore = address(this).balance;
        uint stepType = initStep.stepType;
        bytes memory stepsData = abi.encode(steps);
        
        if ((stepType & 1) == 0) {
            // V2
            uint amount0Out = ((stepType & 2) != 0) ? 0 : input;
            uint amount1Out = ((stepType & 2) != 0) ? input : 0;
            IUniswapV2Pair(initStep.poolAddress).swap(amount0Out, amount1Out, address(this), abi.encode(initStep.token0, initStep.token1, true, stepsData));
        } else {
            // V3
            bool zeroForOne = (stepType & 2) != 0;
            IUniswapV3Pool(initStep.poolAddress).swap(address(this), zeroForOne, int256(input), 1 << 112, abi.encode(initStep.token0, initStep.token1, true, zeroForOne, stepsData));
        }
        
        IWETH weth = IWETH(0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2);
        uint256 wethBalance = weth.balanceOf(address(this));
        weth.withdraw(wethBalance);
        
        uint bribe = fees >> 128;
        block.coinbase.transfer(bribe);
        
        uint gasFee = fees & ((1 << 128) - 1);
        require(balanceBefore + gasFee < address(this).balance, "Insufficient balance to cover gas fee");
    }

    function withdrawFunds() external onlyOwner {
        payable(owner).transfer(address(this).balance);
    }

    receive() external payable {}
}

function swapExactInputForOutput(
    address pairAddress,
    address tokenIn,
    address tokenOut,
    uint inputAmount
) external {
    // 1. Transfer the input token to this contract
    require(IERC20(tokenIn).transferFrom(msg.sender, address(this), inputAmount), "Transfer failed");

    // 2. Approve the Uniswap V2 Pair to spend the input token
    require(IERC20(tokenIn).approve(pairAddress, inputAmount), "Approve failed");

    // 3. Get the reserves of the pair
    (uint reserve0, uint reserve1,) = IUniswapV2Pair(pairAddress).getReserves();
    (uint reserveIn, uint reserveOut) = tokenIn == IUniswapV2Pair(pairAddress).token0() ? (reserve0, reserve1) : (reserve1, reserve0);

    // 4. Calculate the output amount
    uint amountInWithFee = inputAmount * 997;
    uint numerator = amountInWithFee * reserveOut;
    uint denominator = (reserveIn * 1000) + amountInWithFee;
    uint outputAmount = numerator / denominator;

    // 5. Determine the amount0Out and amount1Out
    (uint amount0Out, uint amount1Out) = tokenIn == IUniswapV2Pair(pairAddress).token0() ? (uint(0), outputAmount) : (outputAmount, uint(0));

    // 6. Perform the swap
    IUniswapV2Pair(pairAddress).swap(amount0Out, amount1Out, msg.sender, new bytes(0));
}