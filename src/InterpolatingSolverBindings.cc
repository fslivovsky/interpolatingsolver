#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "InterpolatingSolver.h"

namespace py = pybind11;

namespace pybind11::detail {
    template <> struct type_caster<boost::tribool> {
    public:
        PYBIND11_TYPE_CASTER(boost::tribool, _("tribool"));
        /**
         * Conversion part 2 (C++ -> Python): convert an inty instance into
         * a Python object. The second and third arguments are used to
         * indicate the return value policy and parent object (for
         * ``return_value_policy::reference_internal``) and are generally
         * ignored by implicit casters.
         */
        static handle cast(boost::tribool src, return_value_policy /* policy */, handle /* parent */) {
            if (!src) {
                return PyLong_FromLong(0);
            } else if (src) {
                return PyLong_FromLong(1);
            } else {
                return PyLong_FromLong(2);
            }
        }
    };
}


PYBIND11_MODULE(itp, m) {
    py::class_<InterpolatingSolver>(m, "InterpolatingMiniSAT")
        .def(py::init<int>())
        .def("resetSolver", py::overload_cast<int>(&InterpolatingSolver::resetSolver))
        .def("resetSolver", py::overload_cast<>(&InterpolatingSolver::resetSolver))
        .def("reserve", &InterpolatingSolver::reserve)
        .def("addClause", &InterpolatingSolver::addClause)
        .def("solve", &InterpolatingSolver::solve)
        .def("getConflict", &InterpolatingSolver::getConflict)
        .def("getVarVal", &InterpolatingSolver::getVarVal)
        .def("getInterpolant", &InterpolatingSolver::getInterpolant)
        .def("getDefinitions", &InterpolatingSolver::getDefinitions)
        .def("getDefinition", &InterpolatingSolver::getDefinition)
        .def("interrupt", &InterpolatingSolver::interrupt)
        .def("addFormula", &InterpolatingSolver::addFormula);
}