// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formats/ascii.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/Schedule.hpp>
#include <GREM/execution/Task.hpp>

namespace grem::execution {

namespace detail {

namespace {

void appendQuotedDOTEscapedID(String& output, CStringView string) {
	output.push_back('\"');
	for (const char ch : string) {
		if (ch == '\"') {
			output.append("\\\"");
		} else if (ch == '\\') {
			output.append("\\\\");
		} else {
			output.push_back(ch);
		}
	}
	output.push_back('\"');
}

} // namespace

String formatScheduleDOTGraph(Span<const Task> tasks, CStringView name) {
	String result{"digraph"};
	if (!name.empty()) {
		result.push_back(' ');
		appendQuotedDOTEscapedID(result, name);
	}
	result.append(" {");
	for (size_t i = 0; i < tasks.size(); ++i) {
		result.append(formatString("\n    t{} [label=", i));
		const CStringView taskName = tasks[i].getName();
		if (taskName.empty()) {
			result.append(formatString("\"Task {}\"", i));
		} else {
			appendQuotedDOTEscapedID(result, taskName);
		}
		result.append("];");
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
