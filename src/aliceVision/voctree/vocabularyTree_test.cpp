// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/voctree/Database.hpp>

#include <cereal/archives/binary.hpp>

#include <iostream>
#include <fstream>
#include <vector>

#define BOOST_TEST_MODULE vocabularyTree
#include <boost/test/included/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

using namespace std;
using namespace aliceVision::voctree;

int card_documents = 10;
int card_words = 12;

BOOST_AUTO_TEST_CASE(database_databaseIO) {

  // Create a documents vector
  vector< vector<Word> > documents_to_insert;
  documents_to_insert.resize(card_documents);
  for(int i = 0; i < documents_to_insert.size(); ++i)
  {
    documents_to_insert[i].resize(card_words);
    for(int j = 0; j < card_words; ++j)
    {
      documents_to_insert[i][j] = card_words * i + j;
    }
  }

  // Create the databases
  Database source_db( documents_to_insert.size() * documents_to_insert[0].size() ) ;
  for(int i = 0; i < documents_to_insert.size(); ++i)
  {
    SparseHistogram histo;
    computeSparseHistogram(documents_to_insert[i], histo);
    source_db.insert(i, histo);
  }

  // Compute weights
  source_db.computeTfIdfWeights( );

  // Save the database on disk
  ofstream os("test_database.db");
  cereal::BinaryOutputArchive oarchive(os);
  oarchive(source_db);
  os.close();

  // Load the database saved on the disk
  Database reload_db;
  ifstream is("test_database.db");
  cereal::BinaryInputArchive iarchive(is);
  iarchive(reload_db);
  is.close();

  // Check databases size
  BOOST_CHECK_EQUAL(source_db.size(), reload_db.size());

  // Check returned matches for a given document
  for(int i = 0; i < documents_to_insert.size(); i++)
  {
    // Create match vectors
    vector<DocMatch> source_match(1), reload_match(1);
    // Query both databases with the same document
    source_db.find(documents_to_insert[i], 1, source_match, "classic");
    reload_db.find(documents_to_insert[i], 1, reload_match, "classic");
    // Check we have the same match
    BOOST_CHECK_EQUAL(source_match[0].id, reload_match[0].id);
    // Check the matches scores are 0 (or near)
    BOOST_CHECK_SMALL(static_cast<double>(source_match[0].score), 0.001);
    BOOST_CHECK_SMALL(static_cast<double>(reload_match[0].score), 0.001);
  }
}
