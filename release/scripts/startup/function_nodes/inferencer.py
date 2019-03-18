class Inferencer:
    def __init__(self, type_infos):
        self.type_infos = type_infos
        self.finalized_ids = dict()
        self.constraints = set()
        self.decisions = dict()

    def insert_final_type(self, id, data_type):
        self.finalized_ids[id] = data_type

    def insert_equality_constraint(self, ids, decision_id=None):
        constraint = EqualityConstraint(ids, decision_id)
        self.constraints.add(constraint)

    def insert_list_constraint(self, list_ids, base_ids=tuple(), decision_id=None):
        constraint = ListConstraint(list_ids, base_ids, decision_id, self.type_infos)
        self.constraints.add(constraint)

    def insert_union_constraint(self, ids, allowed_types, decision_id=None):
        constraint = UnionConstraint(ids, decision_id, allowed_types)
        self.constraints.add(constraint)

    def finalize_id(self, id, data_type):
        if id in self.finalized_ids:
            if self.finalized_ids[id] != data_type:
                msg = f"Conflicting type for {id}: {self.finalized_ids[id]} vs. {data_type}"
                raise ConflictingTypesError(msg)
        else:
            self.finalized_ids[id] = data_type

    def finalize_ids(self, ids, data_type):
        for id in ids:
            self.finalize_id(id, data_type)

    def make_decision(self, decision_id, value):
        assert decision_id not in self.decisions
        self.decisions[decision_id] = value

    def inference(self):
        '''
        Tries to find a solution for all constraints.
        Raises InferencingError when it is impossible
        to find a solutions.
        Returns True, when every id has been mapped to
        a data type. Otherwise False.
        '''
        while len(self.constraints) > 0:
            handled_constraints = set()

            for constraint in self.constraints:
                if constraint.try_finalize(self.finalized_ids, self.finalize_ids, self.make_decision):
                    handled_constraints.add(constraint)

            if len(handled_constraints) == 0:
                return False

            self.constraints -= handled_constraints

        return True

    def get_final_type(self, id):
        return self.finalized_ids.get(id, None)

    def get_decisions(self):
        return self.decisions


class Constraint:
    def try_finalize(self, finalized_ids, do_finalize, make_decision):
        raise NotImplementedError()

class EqualityConstraint(Constraint):
    def __init__(self, ids, decision_id):
        self.ids = set(ids)
        self.decision_id = decision_id

    def try_finalize(self, finalized_ids, finalize_do, make_decision):
        for id in self.ids:
            if id in finalized_ids:
                data_type = finalized_ids[id]
                finalize_do(self.ids, data_type)
                if self.decision_id is not None:
                    make_decision(self.decision_id, data_type)
                return True
        return False

class UnionConstraint(Constraint):
    def __init__(self, ids, decision_id, allowed_types):
        self.ids = set(ids)
        self.decision_id = decision_id
        self.allowed_types = set(allowed_types)

    def try_finalize(self, finalized_ids, finalize_do, make_decision):
        for id in self.ids:
            if id in finalized_ids:
                data_type = finalized_ids[id]
                if data_type not in self.allowed_types:
                    raise InferencingError()
                finalize_do(self.ids, data_type)
                if self.decision_id is not None:
                    make_decision(self.decision_id, data_type)
                return True
        return False

class ListConstraint(Constraint):
    def __init__(self, list_ids, base_ids, decision_id, type_infos):
        self.list_ids = set(list_ids)
        self.base_ids = set(base_ids)
        self.decision_id = decision_id
        self.type_infos = type_infos

    def try_finalize(self, finalized_ids, finalize_do, make_decision):
        for id in self.list_ids:
            if id in finalized_ids:
                list_type = finalized_ids[id]
                if not self.type_infos.is_list(list_type):
                    raise NoListTypeError()
                base_type = self.type_infos.to_base(list_type)
                finalize_do(self.list_ids, list_type)
                finalize_do(self.base_ids, base_type)
                if self.decision_id is not None:
                    make_decision(self.decision_id, base_type)
                return True
        for id in self.base_ids:
            if id in finalized_ids:
                base_type = finalized_ids[id]
                if not self.type_infos.is_base(base_type):
                    raise NoBaseTypeError()
                list_type = self.type_infos.to_list(base_type)
                finalize_do(self.base_ids, base_type)
                finalize_do(self.list_ids, list_type)
                if self.decision_id is not None:
                    make_decision(self.decision_id, base_type)
                return True
        return False

class InferencingError(Exception):
    pass

class ConflictingTypesError(InferencingError):
    pass

class NoListTypeError(InferencingError):
    pass

class NoBaseTypeError(InferencingError):
    pass