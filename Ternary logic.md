# Ternary Logic Instead of Binary: How to Make AI Honestly Say "I Don't Know"

## The Problem

Most conversational systems—including large language models—operate on a hidden binary logic: the system either answers with absolute confidence or stays silent. The intermediate state of "I only partially understood the request, and here is why" almost never exists as a distinct, manageable behavioral branch.

Consider a simple example:

> "I want to buy a good phone, help me"

Formally, this is a grammatically correct, complete question. But it is substantively incomplete: it remains unclear what "good" means (camera? battery life? price?), and whether you need help choosing a specific model or navigating the purchase itself. A standard system either guesses (and often misses) or outputs a generic list of general advice that fails to solve the user's specific problem.

The issue isn't the quality of text generation—modern models write fluently regardless. The issue is that the system does not distinguish between "I am certain" and "I am merely assuming," and this distinction is nowhere formally tracked, meaning it cannot be used to govern the system's behavior.

## What If We Introduce a Third State?

Instead of traditional binary logic (yes/no, answer/don't answer), we can introduce a ternary approach:

L3 ∈ {-1, 0, +1}

+1 — state is defined, proceed with action
 0 — state is undefined, a clarification step is required
-1 — conflict or error detected, correction required

This is not a fundamentally new idea—ternary logic is well-established, and its mathematical optimality (in terms of information representation density) is strictly proven: the most economical integer base for a number system is closest to Euler's number e ≈ 2.718, which is precisely 3. On this exact principle, the working ternary computer "Setun" was built at Moscow State University in 1958, serving as a practical rather than purely theoretical proof of the concept's viability.

The fascinating question is whether this same principle can be applied not to hardware computations, but to the actual process of LLM response generation: moving beyond merely generating text to purposefully modifying the task state of the user, explicitly tracking when the system is certain, when it doubts, and when it encounters a contradiction.

## Architecture: Two Control Layers

The system discussed here (provisionally named Block X) is built around two key control points:

L3_in — evaluates the readiness of the incoming prompt: is there enough information to begin solving the problem at all?

L3_out — evaluates the quality of the final solution: is it genuinely defined and useful, or merely generated?

Between them lies all the substantive work: parsing possible interpretations of the prompt, constructing a solution space, and most importantly — verifying discrepancies among independent evaluations before outputting anything to the user.

### Key Mechanism: Detecting Disagreement, Not Just Averaging

Most systems that possess any sort of "consensus" mechanism among multiple evaluations simply average them out. This is dangerous: averaging conceals disagreement. Three evaluations of [0.5, 0.5, 0.5] and three evaluations of [0.1, 0.5, 0.9] yield the exact same mean (0.5), yet these represent fundamentally different situations—in the first case, everyone agrees on a neutral rating; in the second, opinions diverge diametrically.

Here is an implementation that explicitly distinguishes these cases:

import statistics

def rashomon_evaluate(opinions, disagreement_threshold=0.25, labels=None):
    """
    opinions: list of independent evaluations in the range [0, 1]
    disagreement_threshold: threshold for detecting opinion divergence

    Returns not just the mean, but an explicit disagreement flag
    that dictates the system's subsequent behavior.
    """
    if not opinions:
        return None

    n = len(opinions)
    consensus = sum(opinions) / n
    stdev = statistics.pstdev(opinions) if n > 1 else 0.0

    disagreement = stdev > disagreement_threshold

    return {
        "Consensus": consensus,
        "StdDev": stdev,
        "Count": n,
        "Disagreement": disagreement,
        "Recommendation": "FORCE_ANALYZE" if disagreement else "USE_CONSENSUS"
    }

If Disagreement=True, the system is strictly prohibited from silently using the average value, no matter how confident it may look. This forcefully lowers the final solution state (L3), even if the formal "average score" appears high.

## Live Example: The System Caught Its Own Error

The best illustration of this approach's value is not a hypothetical scenario, but a real case encountered during testing.

Task: Compress a well-known literary excerpt (several hundred words) by multiple factors while preserving core meaning. Three compression variants were prepared—ranging from minimal plot summary (heavy compression, ~10x) to detailed retelling (light compression, ~2.5x). Each variant was evaluated across three independent criteria (plot retention, semantic depth preservation, stylistic consistency) using the mechanism described above.

The initial version of the "best choice" selection code relied solely on the efficiency formula (value/cost) and selected the shortest variant—the exact one that almost entirely lost the original semantic depth. Formally, it had the highest efficiency score.

However, the data showed: this exact variant triggered Disagreement=True—independent evaluations across the three criteria diverged sharply (plot was preserved well, but meaning and style were not). The system (via output re-analysis) detected that the selection mechanism had ignored its own warning signal. After introducing an explicit filter—disqualifying any variant with a recorded disagreement from being chosen as "best"—the selection automatically shifted to a balanced variant that preserved both plot and semantic depth at a reasonable (non-maximum) compression rate.

This is not an abstract example of "machine learning"—it is a concrete, reproducible instance where an embedded system defense mechanism corrected a logic error made by the author himself.

## Self-Adaptation Principles in the Architecture

An important caveat upfront: this is not gradient-based learning in the classical machine learning sense (no neural network weights are updated here). This is structured experience accumulation and threshold adaptation—a mechanism of symbolic rather than statistical self-learning. For a technical audience, this distinction is fundamental and must be stated explicitly rather than conflating terms.

### 1. Decision Thresholds Depend on Information Novelty

Standard systems use fixed thresholds: for instance, "confidence > 0.7 — proceed". But identical confidence in a familiar situation and a completely novel one should not be treated the same way.

We introduce a novelty coefficient H_i ∈ [0, 1] which shifts decision thresholds:

def get_thresholds(self, h_i):
    h_i = max(0.0, min(1.0, h_i))
    threshold_high = self.base_threshold_high + self.h_high_shift * h_i
    threshold_low = self.base_threshold_low + self.h_low_shift * h_i
    return {"high": threshold_high, "low": threshold_low}

The more novel and unfamiliar the information, the higher the bar required for the system to accept it with a +1 confidence, and the wider the mandatory verification zone.

### 2. Memory of Successes and Failures as a Growing Rule Base

After each processing cycle, the system classifies the result and stores it in a structured memory:

def save_to_memory(self, knowledge_result, rule_description):
    if knowledge_result["L3"] == +1:
        self.memory["rules"].append(rule_description)
        self.memory["successes"].append(knowledge_result)
        return "SAVED"
    if knowledge_result["L3"] == -1:
        self.memory["failures"].append(knowledge_result)
        return "REJECTED"
    return "PENDING_REVIEW"

### 3. Meta-Evaluation: Auditing the Verification Procedure Itself

A separate and perhaps most compelling principle: the system not only evaluates solutions, but periodically tests the reliability of the evaluation mechanism itself.

def meta_rashomon_evaluate(evaluation_a, evaluation_b, tolerance=0.1):
    consensus_diff = abs(evaluation_a["Consensus"] - evaluation_b["Consensus"])
    disagreement_mismatch = evaluation_a["Disagreement"] != evaluation_b["Disagreement"]

    status = "VALID" if (consensus_diff < tolerance and not disagreement_mismatch) else "REJECTED"
    return {
        "ConsensusDifference": consensus_diff,
        "DisagreementMismatch": disagreement_mismatch,
        "Status": status
    }

### 4. State Delta as a Directed Signal

Every processing cycle concludes by computing ΔL3 = L3_out - L3_in—a directed metric indicating whether processing improved the task state, left it unchanged, or degraded it.

### 5. Rollback as Part of the Cycle, Not a Dead End

Upon detecting a critical error (L3 = -1), the system triggers a protocol that logs the error vector, flushes the failed solution space, and explicitly returns control to the refinement layer with elevated requirements for input data.

## What the System Can Do Today

- Distinguish "certain," "uncertain," and "conflict" as three explicit, distinct behavioral branches rather than hidden heuristics.
- Detect opinion divergence and prevent it from being masked by an average score.
- Explicitly separate two levels of prompt ambiguity: "the subject matter itself is unclear" versus "the subject is clear, but criteria are undefined".
- Formally execute the full cycle: request → readiness analysis → interpretation breakdown → solution space construction → selection with optimization-error protection → post-check → answer generation or explicit rollback.
- Halt not by a fixed step count, but by a substantive criterion: when additional analysis ceases to increase utility relative to its computational cost.

## What the System Cannot Do Yet (Honestly)

- Thresholds are not calibrated on massive datasets.
- "Independent" evaluations are not yet fully independent (often generated by the same source).
- No production deployment (verified only on unit tests).
- Semantic text comparison is currently limited (lexical/structural overlap rather than deep semantic reasoning).

## Where This Can Evolve

Natural next steps include replacing pseudo-independent evaluations with genuinely partitioned sources, calibrating thresholds against real dialogue datasets within specific domains, and conducting load testing under limited pilot conditions.

If this topic interests you from a collaborative development or pilot integration standpoint, reach out via direct messages to discuss details.

---

Implementation relies on several independent modules (opinion divergence handling, semantic matching, solution efficiency scoring, pacing control) integrated into a unified orchestration loop. Full source code available upon request.

Development and testing were conducted in dialogue with Claude (Anthropic)—code was written, executed, and verified iteratively: part of the findings outlined above (including the "live example" case) were discovered precisely through line-by-line execution and result verification rather than purely conceptual design on paper.
