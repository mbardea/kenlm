#include "lm/builder/pipeline.hh"

#include "lm/builder/adjust_counts.hh"
#include "lm/builder/corpus_count.hh"
#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"

#include "util/file.hh"

#include <vector>

namespace lm { namespace builder {

void Pipeline(const PipelineConfig &config, util::FilePiece &text, std::ostream &out) {
  util::scoped_fd vocab_file(config.vocab_file.empty() ? 
      util::MakeTemp(config.TempPrefix()) : 
      util::CreateOrThrow(config.vocab_file.c_str()));

  std::vector<util::stream::ChainConfig> chain_configs(config.order, config.chain);
  for (std::size_t i = 0; i < config.order; ++i) {
    chain_configs[i].entry_size = NGram::TotalSize(i + 1);
  }
  Chains chains(chain_configs);

  chains[config.order - 1] >> CorpusCount(text, config.order, vocab_file.get());

  BlockingSort(chains[config.order - 1], config.sort, SuffixOrder(config.order), AddCombiner());

  std::cerr << "Adjusting" << std::endl;
  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  chains >> AdjustCounts(counts, discounts);

  BlockingSort<ContextOrder>(chains, config.sort);

  std::cerr << "Counts are:\n";
  for (std::size_t i = 0; i < counts.size(); ++i) {
    std::cerr << (i+1) << ' ' << counts[i] << '\n';
  }
  std::cerr << '\n';

  std::cerr << "Printing" << std::endl;
  VocabReconstitute vocab(vocab_file.get());
  chains >> Print<uint64_t>(vocab, out) >> util::stream::kRecycle;
  chains.Wait();
}

}} // namespaces