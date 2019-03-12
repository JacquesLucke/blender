class Inferencer:
    def __init__(self, type_infos):
        self.type_infos = type_infos
        self.finalized_ids = dict()
        self.constraints = set()

    def insert_final_type(self, id, data_type):
        self.finalized_ids[id] = data_type

    def insert_equality_constraint(self, ids):
        constraint = EqualityConstraint(ids)
        self.constraints.add(constraint)

    def insert_list_constraint(self, list_ids, base_ids=tuple()):
        constraint = ListConstraint(list_ids, base_ids, self.type_infos)
        self.constraints.add(constraint)

    def finalize_id(self, id, data_type):
        if id in self.finalized_ids:
            if self.finalized_ids[id] != data_type:
                raise CannotInferenceError()
        else:
            self.finalized_ids[id] = data_type

    def finalize_ids(self, ids, data_type):
        for id in ids:
            self.finalize_id(id, data_type)

    def inference(self):
        while len(self.constraints) > 0:
            handled_constraints = set()

            for constraint in self.constraints:
                if constraint.try_finalize(self.finalized_ids, self.finalize_ids):
                    handled_constraints.add(constraint)

            if len(handled_constraints) == 0:
                raise CannotInferenceError()

            self.constraints -= handled_constraints

    def get_final_type(self, id):
        return self.finalized_ids[id]


class Constraint:
    def try_finalize(self, finalized_ids, do_finalize):
        raise NotImplementedError()

class EqualityConstraint(Constraint):
    def __init__(self, ids):
        self.ids = set(ids)

    def can_be_finalized(self, finalized_ids):
        return any(id in finalized_ids for id in self.ids)

    def try_finalize(self, finalized_ids, finalize_do):
        for id in self.ids:
            if id in finalized_ids:
                finalize_do(self.ids, finalized_ids[id])
                return True
        return False

class ListConstraint(Constraint):
    def __init__(self, list_ids, base_ids, type_infos):
        self.list_ids = set(list_ids)
        self.base_ids = set(base_ids)
        self.type_infos = type_infos

    def try_finalize(self, finalized_ids, finalize_do):
        for id in self.list_ids:
            if id in finalized_ids:
                list_type = finalized_ids[id]
                base_type = self.type_infos.to_base(list_type)
                finalize_do(self.list_ids, list_type)
                finalize_do(self.base_ids, base_type)
                return True
        for id in self.base_ids:
            if id in finalized_ids:
                base_type = finalized_ids[id]
                list_type = self.type_infos.to_list(base_type)
                finalize_do(self.base_ids, base_type)
                finalize_do(self.list_ids, list_type)
                return True
        return False

class CannotInferenceError(Exception):
    ...
