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
}
