# Convert HTML files to text for simple reading
.SUFFIXES: .html .txt

HTML2TXT=lynx -dump

DOCS = readme.txt details.txt 

docs: $(DOCS)

clean:
	rm -f $(DOCS)

.html.txt:
	$(HTML2TXT) $< > $@

