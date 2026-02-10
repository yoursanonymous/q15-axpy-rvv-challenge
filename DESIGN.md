# DESIGN: Q15 AXPY Implementation

## 1. Problem Statement: Why RVV for Q15?
The AXPY operation ($y = \alpha \cdot b + a$) is the fundamental "inner loop" of DSP algorithms. In **Q15 fixed-point arithmetic**, 16-bit integers represent fractions in the range $[-1, 1)$. 

**RISC-V Vector (RVV)** solves performance bottlenecks by processing multiple elements per instruction. By using **LMUL (Length Multiplier)**, we can group vector registers to maximize throughput.

## 2. Mathematical Approaches: Precision vs. Speed

### 2.1 The Widening Path
The highest accuracy for Q15 arithmetic is achieved by performing the multiplication in a wider accumulator.

**Instruction Mix:**
1. `vle16.v` (Load inputs)
2. `vwmul.vx` (16-bit $\to$ 32-bit product)
3. `vsext.vf2` (Sign-extend to 32-bit)
4. `vsll.vx` (Shift left by 15)
5. `vadd.vv` (32-bit sum)
6. `vnclip.wx` (Saturate & Round back to 16-bit)

### 2.2 The Direct vsmul Path
The `vsmul.vx` instruction is a specialized fixed-point operator.

**Instruction Mix:**
1. `vle16.v` (Load inputs)
2. `vsmul.vx` (Fixed-point multiply & shift)
3. `vsadd.vv` (Saturating add)
4. `vse16.v` (Store result)

## 3. Register Grouping: The LMUL Strategy
- **LMUL=1:** Minimizes register footprint. 
- **LMUL=8:** Maximizes throughput by increasing the elements processed per instruction.

## 4. Implementation Mapping
![Godbolt Mapping](screenshots/godbolt_mapping.png)

## 5. Conclusion
The implementation demonstrates a balanced approach between bit-perfect accuracy (Widening) and high-performance throughput (vsmul).
