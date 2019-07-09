from . types_base import (
    DataTypesInfo,
    UniqueSocketBuilder,
    ColoredSocketBuilder,
)

from . sockets import (
    FloatSocket,
    IntegerSocket,
    VectorSocket,
    BooleanSocket,
    ObjectSocket,
    CustomColoredSocket,
)

type_infos = DataTypesInfo()

type_infos.insert_data_type(
    "Float",
    UniqueSocketBuilder(FloatSocket),
    ColoredSocketBuilder((0, 0.3, 0.5, 0.5)))
type_infos.insert_data_type(
    "Vector",
    UniqueSocketBuilder(VectorSocket),
    ColoredSocketBuilder((0, 0, 0.5, 0.5)))
type_infos.insert_data_type(
    "Integer",
    UniqueSocketBuilder(IntegerSocket),
    ColoredSocketBuilder((0.3, 0.7, 0.5, 0.5)))
type_infos.insert_data_type(
    "Boolean",
    UniqueSocketBuilder(BooleanSocket),
    ColoredSocketBuilder((0.3, 0.3, 0.3, 0.5)))
type_infos.insert_data_type(
    "Object",
    UniqueSocketBuilder(ObjectSocket),
    ColoredSocketBuilder((0, 0, 0, 0.5)))

type_infos.insert_conversion_group(["Boolean", "Integer", "Float"])
