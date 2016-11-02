#include "catch.h"
#include "Cache.h"

#include <string.h>
#include <stdlib.h>

#include "Constants.h"
#include "Answer.h"

#include <UDP.h>

using namespace mDNSResolver;
SCENARIO("mDNS packet with a question is received.") {
  GIVEN("the packet has questions") {
    UDP Udp = UDP::loadFromFile("fixtures/question.bin");
    unsigned len = Udp.parsePacket();
    unsigned char *packet = (unsigned char *)malloc(sizeof(unsigned char) * len);

    Udp.read(packet, len);

    WHEN("parsing") {
      unsigned int offset = 0;
      int result = Answer::skipQuestions(packet, len, &offset);

      THEN("questions are skipped") {
        REQUIRE(result == E_MDNS_OK);
      }

      THEN("offset has incremented correctly") {
        // We expect this to be one past the length of the file
        REQUIRE(offset == 45);
      }
    }
  }

  GIVEN("the packet is malicious and attempts to overflow the pointer") {
    UDP Udp = UDP::loadFromFile("fixtures/overflow_question.bin");
    unsigned len = Udp.parsePacket();
    unsigned char *packet = (unsigned char *)malloc(sizeof(unsigned char) * len);

    Udp.read(packet, len);

    WHEN("parsing") {
      unsigned int offset = 0;
      int result = Answer::skipQuestions(packet, len, &offset);

      THEN("an error should be returned") {
        REQUIRE(result == E_MDNS_PACKET_ERROR);
      }
    }
  }
}

SCENARIO("Parsing a name") {
  GIVEN("a DNS encoded name") {
    char name[] = {
      0x04, 't', 'e', 's', 't',
      0x05, 'l', 'o', 'c', 'a', 'l'
    };

    WHEN("parsing") {
      char* result = (char *)malloc(sizeof(char) * 10);
      Answer::parseName(&result, name, 11);

      THEN("converts the name to a FQDN") {
        REQUIRE(strcmp(result, "test.local") == 0);
      }

      free(result);
    }
  }

  GIVEN("a long encoded name") {
    char name[] = {
      0x04, 't', 'h', 'i', 's',
      0x02, 'i', 's',
      0x01, 'a',
      0x04, 't', 'e', 's', 't',
      0x05, 'l', 'o', 'c', 'a', 'l'
    };

    WHEN("parsing") {
      char* result = (char *)malloc(sizeof(char) * 20);
      Answer::parseName(&result, name, 21);

      THEN("converts the name to a FQDN") {
        REQUIRE(strcmp(result, "this.is.a.test.local") == 0);
      }

      free(result);
    }
  }

  GIVEN("a DNS encoded name with an long label length") {
    char name[] = {
      0x3f, 't', 'e', 's', 't',
      0x05, 'l', 'o', 'c', 'a', 'l'
    };

    WHEN("parsing") {
      char* result = (char *)malloc(sizeof(char) * 10);
      int r = Answer::parseName(&result, name, 11);

      THEN("returns an E_MDNS_INVALID_LABEL_LENGTH") {
        REQUIRE(r == E_MDNS_PACKET_ERROR);
      }

      free(result);
    }
  }

  GIVEN("a DNS encoded name with an label length that is shorter than required length, but would cause a buffer overflow") {
    char name[] = {
      0x04, 't', 'e', 's', 't',
      0x07, 'l', 'o', 'c', 'a', 'l'
    };

    WHEN("parsing") {
      char* result = (char *)malloc(sizeof(char) * 10);
      int r = Answer::parseName(&result, name, 11);

      THEN("returns an E_MDNS_INVALID_LABEL_LENGTH") {
        REQUIRE(r == E_MDNS_PACKET_ERROR);
      }

      free(result);
    }
  }
}

SCENARIO("assembling a name") {
  GIVEN("a buffer") {
    unsigned char buffer[] = {
      0x04, 't', 'e', 's', 't',
      0x07, 'l', 'o', 'c', 'a', 'l', 0x00,
      0x05, 't', 'e', 's', 't', '2',
      0xc0, 0x05, // Points at local
      0xc0, 0x2d, // Overflow
      0xc0, 0x0c, // Points at test2.local
    };

    WHEN("assembling a full name") {
      char* result = (char *)malloc(sizeof(char) * MDNS_MAX_NAME_LEN);
      unsigned int offset = 0;
      int len = Answer::assembleName(buffer, 24, &offset, &result);

      THEN("the returned value should be the same as the source length") {
        REQUIRE(len == 12);
      }

      THEN("the name returns the name unchanged") {
        REQUIRE(memcmp(result, buffer, 12) == 0);
      }

      THEN("offset should be incremented") {
        REQUIRE(offset == 12);
      }

      free(result);
    }

    WHEN("assembling a name with a pointer") {
      char* result = (char *)malloc(sizeof(char) * MDNS_MAX_NAME_LEN);
      unsigned int offset = 12;
      int len = Answer::assembleName(buffer, 24, &offset, &result);

      THEN("the returned value should be the same as the source length") {
        REQUIRE(len == 13);
      }

      THEN("resolve the pointer") {
        unsigned char expected[] = {
          0x05, 't', 'e', 's', 't', '2',
          0x07, 'l', 'o', 'c', 'a', 'l', 0x00
        };

        REQUIRE(memcmp(result, expected, 13) == 0);
      }

      THEN("offset should be incremented") {
        REQUIRE(offset == 20);
      }

      free(result);
    }

    WHEN("assembling a name that is a pointer") {
      char* result = (char *)malloc(sizeof(char) * MDNS_MAX_NAME_LEN);
      unsigned int offset = 22;
      int len = Answer::assembleName(buffer, 24, &offset, &result);

      THEN("the returned value should be the same as the source length") {
        REQUIRE(len == 13);
      }

      THEN("resolve the pointer") {
        unsigned char expected[] = {
          0x05, 't', 'e', 's', 't', '2',
          0x07, 'l', 'o', 'c', 'a', 'l', 0x00
        };

        REQUIRE(memcmp(result, expected, 13) == 0);
      }

      THEN("offset should increment by 2") {
        REQUIRE(offset == 24);
      }

      free(result);
    }

    WHEN("assembling a name with an overflowing pointer") {
      char* result = (char *)malloc(sizeof(char) * MDNS_MAX_NAME_LEN);
      unsigned int offset = 20;
      int len = Answer::assembleName(buffer, 24, &offset, &result);

      THEN("the returned -1 * E_MDNS_POINTER_OVERFLOW") {
        REQUIRE(len == -1 * E_MDNS_POINTER_OVERFLOW);
      }

      free(result);
    }
  }

  GIVEN("a packet with an A record") {
    UDP Udp = UDP::loadFromFile("fixtures/cname_answer.bin");
    unsigned len = Udp.parsePacket();
    unsigned char *packet = (unsigned char *)malloc(sizeof(unsigned char) * len);

    Udp.read(packet, len);

    WHEN("parsing") {
      unsigned int offset = 68;
      Answer answer;
      int result = Answer::parseAnswer(packet, len, &offset, &answer);

      THEN("the answer is parsed") {
        REQUIRE(result == E_MDNS_OK);
      }

      THEN("the name should equal a domain") {
        const char* expected = "nas.local";
        REQUIRE(strcmp(expected, (char *)answer.name) == 0);
      }

      THEN("the type should be correct") {
        REQUIRE(answer.type == 1);
      }

      THEN("the class should be correct") {
        REQUIRE(answer.aclass == 1);
      }

      THEN("the flush value should be correct") {
        REQUIRE(answer.cacheflush == 1);
      }

      THEN("the TTL should be correct") {
        REQUIRE(answer.ttl == 120);
      }

      THEN("the data length should be correct") {
        REQUIRE(answer.len == 4);
      }

      THEN("the data should equal the ip address") {
        unsigned char expected[] = {
          0xc0, 0xa8, 0x01, 0x02
        };

        REQUIRE(memcmp(expected, answer.data, 4) == 0);
      }
    }
  }


  GIVEN("a packet with a CNAME") {
    UDP Udp = UDP::loadFromFile("fixtures/cname_answer.bin");
    unsigned len = Udp.parsePacket();
    unsigned char *packet = (unsigned char *)malloc(sizeof(unsigned char) * len);

    Udp.read(packet, len);

    WHEN("parsing") {
      unsigned int offset = 12;
      Answer answer;
      int result = Answer::parseAnswer(packet, len, &offset, &answer);

      THEN("the answer is parsed") {
        REQUIRE(result == E_MDNS_OK);
      }

      THEN("the data should equal another name") {
        const char *expected = "nas.local";
        REQUIRE(memcmp(expected, (char *)answer.data, answer.len) == 0);
      }
    }
  }
}
