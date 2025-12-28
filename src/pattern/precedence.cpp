#include "pattern/precedence.hpp"
#include <iostream>
#include <queue>
#include <algorithm>

namespace tbx {

void PrecedenceRegistry::registerGroup(const std::string& groupName) {
    nodes_.insert(groupName);
}

void PrecedenceRegistry::addRelation(const std::string& tighter, const std::string& looser) {
    // Tighter > Looser
    // We want Higher Priority = Tighter.
    // Topological sort visits dependencies first.
    // If we want P(Tighter) > P(Looser), then Tighter depends on Looser? No.
    // Let's use the Edge definition: Source -> Dest means Source has LOWER priority than Dest.
    // So: Looser -> Tighter.
    nodes_.insert(tighter);
    nodes_.insert(looser);
    adj_[looser].push_back(tighter);
}

void PrecedenceRegistry::assignPatternToGroup(const std::string& patternSignature, const std::string& groupName) {
    nodes_.insert(patternSignature);
    nodes_.insert(groupName);
    patternToGroup_[patternSignature] = groupName;
    
    // Pattern effectively has SAME priority as group
    // For sorting purposes, we can consider them linked in a way that checks the group's val.
    // But simplified: We map Pattern -> Group. calculatePriorities computes Group scores.
    // getPriority looks up Pattern -> Group -> Score.
}

void PrecedenceRegistry::addPatternRelation(const std::string& patternSignature, Relation rel, const std::string& targetSyntax) {
    // Canonicalize target just in case
    // (Caller usually passes raw string from `priority: before "raw string"`)
    // But here we treat targetSyntax as the node key.
    nodes_.insert(patternSignature);
    nodes_.insert(targetSyntax);

    if (rel == Relation::Before) {
        // patternSignature is Tighter ("Before") targetSyntax
        // target -> pattern
        adj_[targetSyntax].push_back(patternSignature);
    } else {
        // patternSignature is Looser ("After") targetSyntax
        // pattern -> target
        adj_[patternSignature].push_back(targetSyntax);
    }
}

bool PrecedenceRegistry::calculatePriorities() {
    priorities_.clear();
    
    // Calculate in-degrees
    std::unordered_map<std::string, int> inDegree;
    for (const auto& node : nodes_) {
        inDegree[node] = 0;
    }
    
    for (const auto& entry : adj_) {
        for (const auto& neighbor : entry.second) {
            inDegree[neighbor]++;
        }
    }

    // Queue for nodes with 0 in-degree (Lowest Priority)
    std::queue<std::string> q;
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            q.push(pair.first);
            priorities_[pair.first] = 0; // Base priority
        }
    }

    int visitedCount = 0;
    
    // Topological Sort (Kahn's Algorithm)
    // We want to assign levels. Nodes at same depth in DAG can have same priority?
    // Pratt parsing usually needs distinct levels for left/right associativity? 
    // But distinct groups is fine.
    
    // Actually, we want to increment priority as we traverse.
    // But simple traversal doesn't guarantee layered levels correctly if multiple branches.
    // Layer assignment: Longest path algorithm? 
    // Since it's a DAG, we can find longest path to each node.
    // Since we're doing Topo Order, we can just update max(parent_prio) + 1.
    
    while (!q.empty()) {
        std::string u = q.front();
        q.pop();
        visitedCount++;

        int p_u = priorities_[u];

        // For each neighbor v of u (u -> v, so u is Looser, v is Tighter)
        if (adj_.count(u)) {
            for (const auto& v : adj_.at(u)) {
                // p[v] = max(p[v], p[u] + 1)
                // Default-constructed map int is 0, which is fine as base.
                if (priorities_.find(v) == priorities_.end()) {
                    priorities_[v] = p_u + 10; // Increment by 10 to leave gaps
                } else {
                    priorities_[v] = std::max(priorities_[v], p_u + 10);
                }

                inDegree[v]--;
                if (inDegree[v] == 0) {
                    q.push(v);
                }
            }
        }
    }

    return visitedCount == nodes_.size(); // False if cycle detected
}

std::optional<int> PrecedenceRegistry::getPriority(const std::string& patternSignature) const {
    // 1. Check if pattern itself has a score
    if (priorities_.count(patternSignature)) {
        return priorities_.at(patternSignature);
    }

    // 2. Check if pattern belongs to a group, and group has a score
    if (patternToGroup_.count(patternSignature)) {
        std::string group = patternToGroup_.at(patternSignature);
        if (priorities_.count(group)) {
            return priorities_.at(group);
        }
    }

    return std::nullopt;
}

std::string PrecedenceRegistry::canonicalize(const std::vector<PatternElement>& elements) {
    std::string sig;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) sig += " ";
        if (elements[i].is_param) {
            sig += "$";
        } else {
            sig += elements[i].value;
        }
    }
    return sig;
}

std::string PrecedenceRegistry::canonicalize(const std::vector<std::string>& rawSyntax) {
    // Raw syntax is already a list of tokens/strings.
    // We need to guess what is a parameter?
    // The raw_syntax vector comes from the parser reading the definition line.
    // E.g. "set", "var", "to", "val".
    // We assume lower-case simple words that are NOT reserved are params?
    // Or we rely on the parser having identified params.
    // Wait, PrecedenceRegistry::canonicalize is called usually AFTER parsing, 
    // or when converting "before $ + $" string.
    
    std::string sig;
    for (size_t i = 0; i < rawSyntax.size(); ++i) {
        if (i > 0) sig += " ";
        sig += rawSyntax[i];
    }
    return sig;
}

} // namespace tbx
