#include "tree.hpp"
#include <memory>
#include <unordered_map>
#include <fstream>
#include <regex>

// 更新escape_label函数以支持自动换行
std::string escape_html_label(const std::string& s, int max_chars_per_line = 20) {
  std::string result = s;
  // 替换HTML特殊字符
  result = std::regex_replace(result, std::regex("&"), "&amp;");
  result = std::regex_replace(result, std::regex("<"), "&lt;");
  result = std::regex_replace(result, std::regex(">"), "&gt;");
  result = std::regex_replace(result, std::regex("\""), "&quot;");
  
  // 尝试在适当的位置添加换行符
  std::string wrapped;
  int line_length = 0;
  for (size_t i = 0; i < result.length(); ++i) {
    if (result[i] == ' ' && line_length > max_chars_per_line) {
      // 在空格处添加换行
      wrapped += "<BR/>";
      line_length = 0;
    } else {
      wrapped += result[i];
      line_length++;
      // 如果遇到逗号、句号等也考虑换行
      if (result[i] == ',' || result[i] == ':' || result[i] == ';') {
        if (line_length > max_chars_per_line / 2) {
          wrapped += "<BR/>";
          line_length = 0;
        }
      }
    }
  }
  return wrapped;
}

void AST::Node::print_tree(std::string prefix, std::string info_prefix) {
  std::cout << info_prefix << to_string() << " (line " << lineno << ")"
            << std::endl;
  auto children = get_children();
  if (children.size() == 1) {
    children[0]->print_tree(prefix + "    ", prefix + " └─ ");
  } else {
    for (size_t i = 0; i < children.size(); i++) {
      if (i == children.size() - 1) {
        children[i]->print_tree(prefix + "    ", prefix + " └─ ");
      } else {
        children[i]->print_tree(prefix + " │  ", prefix + " ├─ ");
      }
    }
  }
}

// 修改draw_ast方法，使用HTML标签格式的标签
void AST::Node::draw_ast(NodePtr root, const std::string& filename) {
  if (!root) {
    std::cerr << "Error: Cannot draw null AST." << std::endl;
    return;
  }
  
  std::ofstream out(filename);
  if (!out.is_open()) {
    std::cerr << "Error: Cannot open file: " << filename << std::endl;
    return;
  }
  
  // 修改图形布局设置，使用HTML标签支持自动换行
  out << "digraph ws {\n";
  out << "    rankdir = TB;\n";      // 垂直布局 (Top to Bottom)
  out << "    nodesep = 0.6;\n";     // 节点间的水平间距
  out << "    ranksep = 1.5;\n";     // 节点间的垂直间距
  // 不再使用fixedsize=true，让节点可以根据内容调整大小
  out << "    node[shape=box, fontsize=18, style=\"filled,bold\", margin=\"0.3,0.2\", width=3.5];\n";
  out << "    edge[penwidth=1.5];\n";  // 加粗边线
  
  std::unordered_map<NodePtr, int> node_map;
  int next_id = 0;
  
  // 递归处理节点
  std::function<void(NodePtr)> process_node = [&](NodePtr node) {
    if (!node) return;
    
    int id = next_id++;
    node_map[node] = id;
    
    std::string node_name = "node" + std::to_string(id);
    
    // 使用HTML标签格式的标签
    std::string label = escape_html_label(node->FormalInfo(), 25);  // 每行最多25个字符
    
    out << "    node[color=\"" << node->Color() << "\", fillcolor=\"" << node->Color() << "20\"];\n";
    out << "    " << node_name << "[label=<" << label << ">];\n";  // 注意这里使用<>而不是引号
    
    // 处理子节点
    for (auto& child : node->get_children()) {
      if (!child) continue;
      
      // 如果子节点还没有被处理，则先处理
      if (node_map.find(child) == node_map.end()) {
        process_node(child);
      }
      
      // 添加边
      out << "    " << node_name << " -> " << "node" << node_map[child] << ";\n";
    }
  };
  
  process_node(root);
  out << "}\n";
  
  out.close();
  std::cout << "AST graph written to " << filename << std::endl;
  std::cout << "Use 'dot -Tpng " << filename << " -o output.png -Gdpi=300' to generate high-resolution image" << std::endl;
}