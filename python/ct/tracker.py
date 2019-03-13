from . import libct


class Tracker:

    def __init__(self):
        self.tracker = libct.tracker_create()

    def __del__(self):
        self.destroy()

    def destroy(self):
        if self.tracker is not None:
            libct.tracker_destroy(self.tracker)
            self.tracker = None

    def lower_bound(self):
        return libct.tracker_lower_bound(self.tracker)

    def upper_bound(self):
        return libct.tracker_upper_bound(self.tracker)

    def run(self, max_iterations=1000):
        libct.tracker_run(self.tracker, max_iterations)

    def forward_step(self, timestep):
        libct.tracker_forward_step(self.tracker, timestep)

    def backward_step(self, timestep):
        libct.tracker_backward_step(self.tracker, timestep)


def construct_tracker(model):
    t = Tracker()
    g = libct.tracker_get_graph(t.tracker)

    detection_map = {} # (timestep, detection) -> detection_object
    for timestep in range(model.no_timesteps()):
        # FIXME: This is not very efficient.
        conflict_counter = {}
        conflict_slots = {}
        for conflict in range(model.no_conflicts(timestep)):
            detections = model._conflicts[timestep, conflict]
            for d in detections:
                conflict_slots[conflict, d] = conflict_counter.get(d, 0)
                model._inc_dict(conflict_counter, d)

        for detection in range(model.no_detections(timestep)):
            d = libct.graph_add_detection(
                g, timestep, detection, model.no_incoming_edges(timestep,
                detection), model.no_outgoing_edges(timestep, detection),
                conflict_counter.get(detection, 0))

            c_det, c_app, c_dis = model._detections[timestep, detection]
            libct.detection_set_detection_cost(d, c_det)
            libct.detection_set_appearance_cost(d, c_app)
            libct.detection_set_disappearance_cost(d, c_dis)
            detection_map[timestep, detection] = d

        for conflict in range(model.no_conflicts(timestep)):
            detections = model._conflicts[timestep, conflict]
            c = libct.graph_add_conflict(g, timestep, conflict, len(detections))
            for i, d in enumerate(detections):
                conflict_count = conflict_counter[d]
                assert conflict_count >= 1
                libct.graph_add_conflict_link(g, timestep, conflict, i, d, conflict_slots[conflict, d])
                conflict_counter[d] = conflict_count - 1

        if __debug__:
            for k, v in conflict_counter.items():
                assert v == 0

    for k, v in model._transitions.items():
        timestep, index_from, index_to = k
        slot_left, slot_right, cost = v

        libct.detection_set_outgoing_cost(detection_map[timestep, index_from], slot_left, cost * .5)
        libct.detection_set_incoming_cost(detection_map[timestep + 1, index_to], slot_right, cost * .5)
        libct.graph_add_transition(g, timestep, index_from, slot_left, index_to, slot_right)

    for k, v in model._divisions.items():
        timestep, index_from, index_to_1, index_to_2 = k
        slot_left, slot_right_1, slot_right_2, cost = v

        libct.detection_set_outgoing_cost(detection_map[timestep, index_from], slot_left, cost / 3.0)
        libct.detection_set_incoming_cost(detection_map[timestep + 1, index_to_1], slot_right_1, cost / 3.0)
        libct.detection_set_incoming_cost(detection_map[timestep + 1, index_to_2], slot_right_2, cost / 3.0)
        libct.graph_add_division(g, timestep, index_from, slot_left, index_to_1, slot_right_1, index_to_2, slot_right_2)

    libct.tracker_finalize(t.tracker)

    return t
