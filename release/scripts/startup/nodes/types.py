from . types_base import (
    DataTypesInfo,
    UniqueSocketBuilder,
)

from . sockets import (
    FloatSocket,
    IntegerSocket,
    VectorSocket,
    BooleanSocket,
    ObjectSocket,
    FloatListSocket,
    VectorListSocket,
    IntegerListSocket,
    BooleanListSocket,
    ObjectListSocket,
)

type_infos = DataTypesInfo()

type_infos.insert_data_type(
    "Float",
    UniqueSocketBuilder(FloatSocket),
    UniqueSocketBuilder(FloatListSocket))
type_infos.insert_data_type(
    "Vector",
    UniqueSocketBuilder(VectorSocket),
    UniqueSocketBuilder(VectorListSocket))
type_infos.insert_data_type(
    "Integer",
    UniqueSocketBuilder(IntegerSocket),
    UniqueSocketBuilder(IntegerListSocket))
type_infos.insert_data_type(
    "Boolean",
    UniqueSocketBuilder(BooleanSocket),
    UniqueSocketBuilder(BooleanListSocket))
type_infos.insert_data_type(
    "Object",
    UniqueSocketBuilder(ObjectSocket),
    UniqueSocketBuilder(ObjectListSocket))

type_infos.insert_conversion_group(["Boolean", "Integer", "Float"])
