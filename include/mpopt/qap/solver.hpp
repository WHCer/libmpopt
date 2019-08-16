#ifndef LIBMPOPT_QAP_SOLVER_HPP
#define LIBMPOPT_QAP_SOLVER_HPP

namespace mpopt {
namespace qap {

template<typename ALLOCATOR>
class solver : public ::mpopt::solver<solver<ALLOCATOR>> {
public:
  using allocator_type = ALLOCATOR;
  using graph_type = graph<allocator_type>;
  using gurobi_model_builder_type = gurobi_model_builder<allocator_type>;

  solver(const ALLOCATOR& allocator = ALLOCATOR())
  : graph_(allocator)
  { }

  auto& get_graph() { return graph_; }
  const auto& get_graph() const { return graph_; }

  void run(const int max_iterations = 1000)
  {
    graph_.check_structure();
    const int max_batches = (max_iterations + batch_size - 1) / batch_size;
    cost best_ub = std::numeric_limits<cost>::infinity();

    signal_handler h;
    using clock_type = std::chrono::high_resolution_clock;
    const auto clock_start = clock_type::now();
    std::cout.precision(std::numeric_limits<cost>::max_digits10);
    for (int i = 0; i < max_batches && !h.signaled(); ++i) {
      for (int j = 0; j < batch_size-1; ++j)
        single_pass<false>();

      single_pass<true>();
      best_ub = std::min(best_ub, this->evaluate_primal());

      const auto clock_now = clock_type::now();
      const std::chrono::duration<double> seconds = clock_now - clock_start;

      const auto lb = this->lower_bound();
      this->iterations_ += batch_size;
      std::cout << "it=" << this->iterations_ << " "
                << "lb=" << lb << " "
                << "ub=" << best_ub << " "
                << "gap=" << static_cast<float>(100.0 * (best_ub - lb) / std::abs(lb)) << "% "
                << "t=" << seconds.count() << std::endl;
    }
  }

  void solve_lap_as_ilp()
  {
    // We do not reset the primals and use the currently set ones as MIP start.
    gurobi_model_builder<allocator_type> builder(this->gurobi_env_);

    for (const auto* node : graph_.unaries())
      builder.add_factor(node);

    for (const auto* node : graph_.uniqueness())
      builder.add_factor(node);

#ifndef NDEBUG
    for (const auto* node : graph_.pairwise())
      assert(dbg::are_identical(node->factor.lower_bound(), 0.0));
#endif

    builder.finalize();
    builder.optimize();
    builder.update_primals();

    for (const auto* node : graph_.pairwise())
      node->factor.primal() = std::tuple(
        node->unary0->factor.primal(),
        node->unary1->factor.primal());
  }

protected:

  template<typename FUNCTOR>
  void for_each_node(FUNCTOR f) const
  {
    for (const auto* node : graph_.unaries())
      f(node);

    for (const auto* node : graph_.uniqueness())
      f(node);

    for (const auto* node : graph_.pairwise())
      f(node);
  }

  template<bool rounding>
  void single_pass()
  {
    for (const auto* node : graph_.pairwise())
      pairwise_messages::update(node);

    if constexpr (rounding)
      solve_lap_as_ilp();

    for (const auto* node : graph_.unaries()) {
      this->constant_ += node->factor.normalize();
      uniqueness_messages::send_messages_to_uniqueness(node);
    }

    for (const auto* node : graph_.uniqueness()) {
      this->constant_ += node->factor.normalize();
      uniqueness_messages::send_messages_to_unaries(node);
    }
  }

  graph_type graph_;
  friend class ::mpopt::solver<solver<ALLOCATOR>>;
};

}
}

#endif

/* vim: set ts=8 sts=2 sw=2 et ft=cpp: */
