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
}
