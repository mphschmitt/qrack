//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2021. All rights reserved.
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#include "pinvoke_api.cpp"
#include <pybind11/pybind11.h>

PYBIND11_MODULE(pureqrack, m)
{
    // Not yet implemented:
    // pybind11::register_exception<std::runtime_error>(module, "QrackRuntimeException");
    // pybind11::register_exception<std::bad_alloc>(module, "QrackBadAllocException");
    // pybind11::register_exception<std::invalid_argument>(module, "QrackInvalidArgumentException");

    m.doc() = "Qrack pybind11 module";

    m.def("init", &init, "");
    m.def("init_count", &init_count, "");
    m.def("init_count_type", &init_count_type, "");
    m.def("init_clone", &init_clone, "");
    m.def("destroy", &destroy, "");
    m.def("seed", &seed, "");
    m.def("set_concurrency", &set_concurrency, "");
    m.def("Prob", &Prob, "");
    m.def("PermutationExpectation", &PermutationExpectation, "");
    m.def("JointEnsembleProbability", &JointEnsembleProbability, "");
    m.def("PhaseParity", &PhaseParity, "");
    m.def("ResetAll", &ResetAll, "");
    m.def("allocateQubit", &allocateQubit, "");
    m.def("release", &release, "");
    m.def("num_qubits", &num_qubits, "");
    m.def("X", &X, "");
    m.def("Y", &Y, "");
    m.def("Z", &Z, "");
    m.def("H", &H, "");
    m.def("S", &S, "");
    m.def("T", &T, "");
    m.def("AdjS", &AdjS, "");
    m.def("AdjT", &AdjT, "");
    m.def("U", &U, "");
    m.def("Mtrx", &Mtrx, "");
    m.def("MCX", &MCX, "");
    m.def("MCY", &MCY, "");
    m.def("MCZ", &MCZ, "");
    m.def("MCH", &MCH, "");
    m.def("MCS", &MCS, "");
    m.def("MCT", &MCT, "");
    m.def("MCAdjS", &MCAdjS, "");
    m.def("MCAdjT", &MCAdjT, "");
    m.def("MCU", &MCU, "");
    m.def("MCMtrx", &MCMtrx, "");
    m.def("MACX", &MACX, "");
    m.def("MACY", &MACY, "");
    m.def("MACZ", &MACZ, "");
    m.def("MACH", &MACH, "");
    m.def("MACS", &MACS, "");
    m.def("MACT", &MACT, "");
    m.def("MACAdjS", &MACAdjS, "");
    m.def("MACAdjT", &MACAdjT, "");
    m.def("MACU", &MACU, "");
    m.def("MACMtrx", &MACMtrx, "");
    m.def("Multiplex1Mtrx", &Multiplex1Mtrx, "");
    m.def("R", &R, "");
    m.def("MCR", &MCR, "");
    m.def("Exp", &Exp, "");
    m.def("MCExp", &MCExp, "");
    m.def("M", &M, "");
    m.def("Measure", &Measure, "");
    m.def("MeasureShots", &MeasureShots, "");
    m.def("SWAP", &SWAP, "");
    m.def("ISWAP", &ISWAP, "");
    m.def("FSim", &FSim, "");
    m.def("CSWAP", &CSWAP, "");
    m.def("ACSWAP", &ACSWAP, "");
    m.def("Compose", &Compose, "");
    m.def("Decompose", &Decompose, "");
    m.def("Dispose", &Dispose, "");
    m.def("AND", &AND, "");
    m.def("OR", &OR, "");
    m.def("XOR", &XOR, "");
    m.def("NAND", &NAND, "");
    m.def("NOR", &NOR, "");
    m.def("XNOR", &XNOR, "");
    m.def("CLAND", &CLAND, "");
    m.def("CLOR", &CLOR, "");
    m.def("CLXOR", &CLXOR, "");
    m.def("CLNAND", &CLNAND, "");
    m.def("CLNOR", &CLNOR, "");
    m.def("CLXNOR", &CLXNOR, "");
    m.def("QFT", &QFT, "");
    m.def("IQFT", &IQFT, "");
    m.def("ADD", &ADD, "");
    m.def("SUB", &SUB, "");
    m.def("ADDS", &ADDS, "");
    m.def("SUBS", &SUBS, "");
    m.def("MUL", &MUL, "");
    m.def("DIV", &DIV, "");
    m.def("MULN", &MULN, "");
    m.def("DIVN", &DIVN, "");
    m.def("POWN", &POWN, "");
    m.def("MCADD", &MCADD, "");
    m.def("MCSUB", &MCSUB, "");
    m.def("MCMUL", &MCMUL, "");
    m.def("MCDIV", &MCDIV, "");
    m.def("MCMULN", &MCMULN, "");
    m.def("MCDIVN", &MCDIVN, "");
    m.def("MCPOWN", &MCPOWN, "");
    m.def("LDA", &LDA, "");
    m.def("ADC", &ADC, "");
    m.def("SBC", &SBC, "");
    m.def("Hash", &Hash, "");
    m.def("TrySeparate1Qb", &TrySeparate1Qb, "");
    m.def("TrySeparate2Qb", &TrySeparate2Qb, "");
    m.def("TrySeparateTol", &TrySeparateTol, "");
    m.def("SetReactiveSeparate", &SetReactiveSeparate, "");
}
