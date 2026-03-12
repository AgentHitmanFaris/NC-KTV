import sys
import os

path = r'd:\\Document\\NC-KTV\\cpp\\src\\gui\\precision\\precision_mode.cpp'
with open(path, 'r', encoding='utf-8') as f:
    code = f.read()

# Update types
code = code.replace('LyricLine', 'core::LyricsLine')
code = code.replace('LyricWord', 'core::LyricsToken')
code = code.replace('.words.isEmpty()', '.tokens.empty()')
code = code.replace('.words.size()', '.tokens.size()')
code = code.replace('.words.first()', '.tokens.front()')
code = code.replace('.words.last()', '.tokens.back()')
code = code.replace('.words[', '.tokens[')
code = code.replace('line.words', 'line.tokens')
code = code.replace('w.word', 'QString::fromStdString(w.text)')
code = code.replace('w.startTime', 'w.start_time')
code = code.replace('w.endTime', 'w.end_time')

# Update other start/endTime cases for lines/words
code = code.replace('.startTime', '.start_time')
code = code.replace('.endTime', '.end_time')

# Fix text vs word
code = code.replace('orig.word', 'QString::fromStdString(orig.text)')
code = code.replace('QString::fromStdString(orig.text) = text.left(midChar);', 'orig.text = text.left(midChar).toStdString();')
code = code.replace('QString::fromStdString(orig.text) = ', 'orig.text = ')

code = code.replace('line.tokens[row].word = item->text();', 'line.tokens[row].text = item->text().toStdString();')
code = code.replace('line.tokens[idx].word = item->text();', 'line.tokens[idx].text = item->text().toStdString();')

code = code.replace('line.text = parts.join(" ");', 'line.text = parts.join(" ").toStdString();')
code = code.replace("line.text = parts.join(' ');", 'line.text = parts.join(" ").toStdString();')
code = code.replace('QString::fromStdString(line.text)', 'QString::fromStdString(line.text)') # prevent dupes
code = code.replace('line.text', 'QString::fromStdString(line.text)')

code = code.replace('.tokens.append({"new_word",', '.tokens.push_back({"new_word",')

# Iterators for insert / erase
code = code.replace('line.tokens.insert(idx + 1, word2);', 'line.tokens.insert(line.tokens.begin() + idx + 1, word2);')
code = code.replace('line.tokens.removeAt(idx + 1);', 'line.tokens.erase(line.tokens.begin() + idx + 1);')
code = code.replace('line.tokens.removeAt(idx);', 'line.tokens.erase(line.tokens.begin() + idx);')

# Fix assignments
code = code.replace('line.tokens[idx].text = text.toStdString();', 'line.tokens[idx].text = text.toStdString();')
code = code.replace('line.tokens[idx].word += " " + line.tokens[idx + 1].word;', 'line.tokens[idx].text += " " + line.tokens[idx + 1].text;')
code = code.replace('QString::fromStdString(line.tokens[idx].text) += " " + QString::fromStdString(line.tokens[idx + 1].text);', 'line.tokens[idx].text += " " + line.tokens[idx + 1].text;')

code = code.replace('QString::fromStdString(orig.text) = text.left(midChar);', 'orig.text = text.left(midChar).toStdString();')
code = code.replace('orig.word', 'QString::fromStdString(orig.text)')

code = code.replace('w.startTime', 'w.start_time')
code = code.replace('w.endTime', 'w.end_time')

# Float casting:
code = code.replace('.tokens.push_back({"new_word", start, start + 0.5});', '.tokens.push_back({"new_word", static_cast<float>(start), static_cast<float>(start + 0.5)});')
code = code.replace('core::LyricsToken word2{text.mid(midChar), midTime, orig.end_time};', 'core::LyricsToken word2{text.mid(midChar).toStdString(), static_cast<float>(midTime), orig.end_time};')
code = code.replace('orig.end_time = midTime;', 'orig.end_time = static_cast<float>(midTime);')

# Double un-replacing
code = code.replace('QString::fromStdString(QString::fromStdString(', 'QString::fromStdString(')
code = code.replace('))', ')')

with open(path, 'w', encoding='utf-8') as f:
    f.write(code)
