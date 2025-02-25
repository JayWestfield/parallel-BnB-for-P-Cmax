/**
 * @brief describes the relativ difficulty of solving that instance
 */
enum class Difficulty {
  /**
   * @brief Lpt is the proovable best solution
   */
  trivial,
  /**
   * @brief Lpt is optimal but does not match the lower bound
   */
  lptOpt,
  /**
   * @brief optimal solution is found by the lower Bound
   */
  lowerBoundOptimal,
  /**
   * @brief optimal solution is found by a non trivial lower Bound
   */
  lowerBoundOptimal2,
  /**
   * @brief full search is necessary to gurantee optimum
   */
  full
};