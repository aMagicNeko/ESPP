import sympy as sp

def get_analytical_solution_for_uniswap(length):
    eqs = []
    k = sp.symbols("k")
    # mid var
    mid_vars = []
    token_in = sp.symbols(f"input")
    prev_out = token_in
    for i in range(length):
        # reserve0
        xi = sp.symbols(f"x{i}")
        # reserve1
        yi = sp.symbols(f"y{i}")
        # input
        dxi = sp.symbols(f"dx{i}")
        # output
        dyi = sp.symbols(f"dy{i}")
        # 1-fee
        ki = sp.symbols(f"k{i}")
        mid_vars.append(dxi)
        mid_vars.append(dyi)
        eq = sp.Eq((xi+ki*dxi)*(yi-dyi)-xi*yi, 0)
        eqs.append(eq)
        eqs.append(sp.Eq(dxi-prev_out, 0))
        prev_out = dyi
    results = sp.solve(eqs, mid_vars)
    print(results)
    token_out = results[0][len(results[0])-1]
    return sp.solve(sp.diff(token_out-token_in, token_in), token_in)

if __name__ == "__main__":
    ret = get_analytical_solution_for_uniswap(6)
    sp.simplify(ret[1])