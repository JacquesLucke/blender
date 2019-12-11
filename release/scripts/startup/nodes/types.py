from . import sockets as s
from . types_base import DataTypesInfo

type_infos = DataTypesInfo()

type_infos.insert_data_type(s.FloatSocket, s.FloatListSocket)
type_infos.insert_data_type(s.VectorSocket, s.VectorListSocket)
type_infos.insert_data_type(s.IntegerSocket, s.IntegerListSocket)
type_infos.insert_data_type(s.BooleanSocket, s.BooleanListSocket)
type_infos.insert_data_type(s.ObjectSocket, s.ObjectListSocket)
type_infos.insert_data_type(s.ImageSocket, s.ImageListSocket)
type_infos.insert_data_type(s.ColorSocket, s.ColorListSocket)
type_infos.insert_data_type(s.TextSocket, s.TextListSocket)
type_infos.insert_data_type(s.SurfaceHookSocket, s.SurfaceHookListSocket)

type_infos.insert_conversion_group(["Boolean", "Integer", "Float"])
