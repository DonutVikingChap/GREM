// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/Schedule.hpp>
#include <GREM/execution/Task.hpp>

namespace grem::execution {

namespace detail {

String getScheduleDOTGraph(Span<const Task> tasks) {
	String result{"digraph schedule {"};
	for (size_t i = 0; i < tasks.size(); ++i) {
		result.append(formatString("\n    t{} [label=\"", i));
		const CStringView name = tasks[i].getName();
		if (name.empty()) {
			result.append(formatString("Task {}", i));
		} else {
			for (const char ch : name) {
				if (ch == '\"') {
					result.append("\\\"");
				} else if (ch == '\\') {
					result.append("\\\\");
				} else {
					result.push_back(ch);
				}
			}
		}
		result.append("\"];");
	}
	result.push_back('\n');
	for (size_t i = 0; i < tasks.size(); ++i) {
		for (const Task::GraphIndex dependencyIndex : tasks[i].getDependencyIndices()) {
			result.append(formatString("\n    t{} -> t{};", dependencyIndex, i));
		}
	}
	result.append("\n}\n");
	return result;
}

} // namespace detail

} // namespace grem::execution
