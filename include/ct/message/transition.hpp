#ifndef LIBCT_MESSAGE_TRANSITION_HPP
#define LIBCT_MESSAGE_TRANSITION_HPP

namespace ct {

struct transition_messages {

  template<bool to_right, typename DETECTION_NODE>
  static void send_messages(const DETECTION_NODE& node)
  {
    auto& here = node.detection;
    using detection_type = typename DETECTION_NODE::detection_type;

    const auto min_other_side   = to_right ? here.min_incoming()
                                           : here.min_outgoing();
    const auto& costs_this_side = to_right ? here.outgoing_
                                           : here.incoming_;
    const auto cost_nirvana     = to_right ? here.disappearance()
                                           : here.appearance();

    const auto constant = here.detection() + min_other_side;
    const auto [first_minimum, second_minimum] = least_two_values(costs_this_side.begin(), costs_this_side.end() - 1);

    const auto set_to = std::min(constant + std::min(second_minimum, cost_nirvana), 0.0);

    index slot = 0;
    for (const auto& edge : node.template transitions<to_right>()) {
      const auto slot_cost   = to_right ? here.outgoing(slot)
                                        : here.incoming(slot);
      const auto repam_this  = to_right ? &detection_type::repam_outgoing
                                        : &detection_type::repam_incoming;
      const auto repam_other = to_right ? &detection_type::repam_incoming
                                        : &detection_type::repam_outgoing;

      auto msg = constant + slot_cost - set_to;
      (here.*repam_this)(slot, -msg);
      if (edge.is_division() && to_right) {
        (edge.node1->detection.*repam_other)(edge.slot1, .5 * msg);
        (edge.node2->detection.*repam_other)(edge.slot2, .5 * msg);
      } else {
        (edge.node1->detection.*repam_other)(edge.slot1, msg);
      }
      ++slot;
    }
  }

  template<typename DETECTION_NODE>
  static consistency check_primal_consistency(const DETECTION_NODE& node)
  {
    const auto& here = node.detection;

    consistency result;
    index slot;

    slot = 0;
    for (const auto& edge : node.incoming) {
      const auto& there1 = edge.node1->detection;
      if (here.primal_.is_incoming_set() && there1.primal_.is_outgoing_set()) {
        if ((here.primal_.incoming() == slot) != (there1.primal_.outgoing() == edge.slot1))
          result.mark_inconsistent();
      } else {
        result.mark_unknown();
      }
      ++slot;
    }

    slot = 0;
    for (auto& edge : node.outgoing) {
      const auto& there1 = edge.node1->detection;
      if (here.primal_.is_outgoing_set() && there1.primal_.is_incoming_set()) {
        if ((here.primal_.outgoing() == slot) != (there1.primal_.incoming() == edge.slot1))
          result.mark_inconsistent();
      } else {
        result.mark_unknown();
      }

      if (edge.node2 != nullptr) {
        const auto& there2 = edge.node2->detection;
        if (here.primal_.is_outgoing_set() && there2.primal_.is_incoming_set()) {
          if ((here.primal_.outgoing() == slot) != (there2.primal_.incoming() == edge.slot2))
            result.mark_inconsistent();
        } else {
          result.mark_unknown();
        }
      }

      ++slot;
    }

    return result;
  }

  template<bool to_right, typename DETECTION_NODE>
  static void propagate_primal(const DETECTION_NODE& node)
  {
    const auto& here = node.detection;

    if (here.primal_.is_detection_off())
      return;

    if constexpr (to_right) {
      assert(here.primal_.is_outgoing_set());
      if (here.primal_.outgoing() < here.outgoing_.size() - 1) {
        const auto& edge = node.outgoing[here.primal_.outgoing()];

        edge.node1->detection.primal_.set_incoming(edge.slot1);
        if (edge.node2 != nullptr)
          edge.node2->detection.primal_.set_incoming(edge.slot2);
      }
    } else {
      assert(here.primal_.is_incoming_set());
      if (here.primal_.incoming() < here.incoming_.size() - 1){
        const auto& edge = node.incoming[here.primal_.incoming()];

        edge.node1->detection.primal_.set_outgoing(edge.slot1);
        if (edge.node2 != nullptr)
          edge.node2->detection.primal_.set_incoming(edge.slot2);
      }
    }
  }

  template<bool from_left, typename DETECTION_NODE, typename CONTAINER>
  static void get_primal_possibilities(const DETECTION_NODE& node, CONTAINER& out)
  {
    out.fill(true);

    auto get_primal = [&](const auto& factor) {
      if constexpr (from_left)
        return factor.primal_.outgoing();
      else
        return factor.primal_.incoming();
    };

    auto get_primal2 = [&](const auto& factor) {
      if constexpr (from_left)
        return factor.primal_.incoming();
      else
        return factor.primal_.outgoing();
    };

    auto it = out.begin();
    for (const auto& edge : node.template transitions<!from_left>()) {
      assert(it != out.end());

      auto helper = [&](const auto& factor, auto slot, auto primal_getter) {
        const auto p = primal_getter(factor);
        if (p != detection_primal::undecided && p != slot)
          *it = false;

        if (p == slot) {
          bool current = *it;
          out.fill(false);
          *it = current;
        }
      };

      helper(edge.node1->detection, edge.slot1, get_primal);
      if (edge.node2 != nullptr)
        if constexpr (from_left)
          helper(edge.node2->detection, edge.slot2, get_primal2);
        else
          helper(edge.node2->detection, edge.slot2, get_primal);
      ++it;
    }

    assert(std::find(out.cbegin(), out.cend(), true) != out.cend());
  }

};

}

#endif

/* vim: set ts=8 sts=2 sw=2 et ft=cpp: */
